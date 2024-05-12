/*
    This file is part of the KDE libraries
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "krun.h"
#include "kstandarddirs.h"
#include "kopenwithdialog.h"
#include "ktoolinvocation.h"
#include "kmimetypetrader.h"
#include "kshell.h"
#include "kdebug.h"

#include <QFileInfo>
#include <QDBusConnectionInterface>

bool KRun::displayOpenWithDialog(const KUrl::List &urls, QWidget *window, bool tempFiles)
{
    KOpenWithDialog dialog(urls, i18n("Open with:"), QString(), window);
    dialog.setWindowModality(Qt::WindowModal);
    if (dialog.exec()) {
        KService::Ptr service = dialog.service();
        if (service) {
            return KToolInvocation::self()->startServiceByStorageId(service->entryPath(), urls.toStringList(), window, tempFiles);
        }
        const QString program = dialog.text();
        if (!program.isEmpty()) {
            return KToolInvocation::self()->startProgram(program, urls.toStringList(), window, tempFiles);
        }
    }
    return false;
}

// TODO: this needs a complete rewrite to handle remote URLs
QStringList KRun::processDesktopExec(const KService &service, const QStringList &urls)
{
    QStringList args = KShell::splitArgs(service.exec());
    if (args.isEmpty()) {
        return args;
    }

    if (service.terminal()) {
        KConfigGroup generalgroup(KGlobal::config(), "General");
        const QString terminal = generalgroup.readPathEntry("TerminalApplication", QLatin1String("konsole"));
        const QString terminalexe = KStandardDirs::findExe(terminal);
        if (terminalexe.isEmpty()) {
            return QStringList();
        }
        args.prepend(QLatin1String("-e"));
        const QStringList terminalargs = KShell::splitArgs(service.terminalOptions());
        foreach (const QString &terminalarg, terminalargs) {
            args.prepend(terminalarg);
        }
        args.prepend(terminalexe);
    }

    if (service.substituteUid()) {
        const QString kdesudoexe = KStandardDirs::findExe("kdesudo");
        if (kdesudoexe.isEmpty()) {
            return QStringList();
        }
        args.prepend(QLatin1String("--"));
        args.prepend(service.username());
        args.prepend(QLatin1String("-u"));
        args.prepend(kdesudoexe);
    }

    QMutableListIterator<QString> iter(args);
    while (iter.hasNext()) {
        QString &arg = iter.next();
        if (arg.contains(QLatin1String("%f")) || arg.contains(QLatin1String("%u"))) {
            if (!urls.isEmpty()) {
                arg = urls.first();
            } else {
                iter.remove();
            }
        } else if (arg.contains(QLatin1String("%F")) || arg.contains(QLatin1String("%U"))) {
            iter.remove();
            foreach (const QString &url, urls) {
                iter.insert(url);
            }
        } else if (arg.contains(QLatin1String("%i"))) {
            arg = service.icon();
        } else if (arg.contains(QLatin1String("%c"))) {
            arg = service.name();
        } else if (arg.contains(QLatin1String("%k"))) {
            arg = service.entryPath();
        }
    }
    return args;
}

QString KRun::binaryName(const QString &execLine, bool removePath)
{
    foreach (const QString &arg, KShell::splitArgs(execLine)) {
        if (!arg.contains('=')) {
            return (removePath ? arg.mid(arg.lastIndexOf('/') + 1) : arg);
        }
    }
    return QString();
}

bool KRun::isExecutable(const QString &serviceType)
{
    return (
        serviceType == QLatin1String("application/x-desktop") ||
        serviceType == QLatin1String("application/x-executable") ||
        serviceType == QLatin1String("application/x-ms-dos-executable") ||
        serviceType == QLatin1String("application/x-shellscript")
    );
}

bool KRun::isExecutableFile(const KUrl &url, const QString &mimetype)
{
    if (isExecutable(mimetype)) {
        return true;
    }
    if (!url.isLocalFile()) {
        return false;
    }
    QFileInfo fileinfo(url.toLocalFile());
    if (!fileinfo.isDir() && fileinfo.isExecutable()) {
        return true;
    }
    return false;
}

bool KRun::checkStartupNotify(const KService *service, QByteArray *wmclass_arg)
{
    if (!service || service->entryPath().isEmpty()) {
        // non-compliant app or service action

        // TODO: for service actions (and other KService's crafted from the name, exec and icon)
        // get the ASN property from the "Desktop Entry" group in the .desktop file somehow
        return false;
    }

    if (service->property("StartupNotify").toBool() != true) {
        // nope
        return false;
    }

    // NOTE: this is using spec properties but probably not for the purpose the properties were
    // ment for
    if (service->property("SingleMainWindow").toBool()) {
        const QStringList implements = service->property("Implements").toStringList();
        if (!implements.isEmpty()) {
            // TODO: where is the interface supposed to be registered?
            QDBusConnectionInterface* dbusconnectioninterface = QDBusConnection::sessionBus().interface();
            if (dbusconnectioninterface) {
                QDBusReply<bool> implementedreply;
                foreach (const QString &implemented, implements) {
                    implementedreply = dbusconnectioninterface->isServiceRegistered(implemented);
                    if (implementedreply.isValid() && implementedreply.value() == true) {
                        kDebug(7010) << "implemented interface" << implemented << ", disabling startup notification";
                        return false;
                    }
                }
            } else {
                kDebug(7010) << "null D-Bus connection interface";
            }
        }
    }

    if (wmclass_arg) {
        *wmclass_arg = service->property("StartupWMClass").toString().toLatin1();
    }

    return true;
}

#include "moc_krun.cpp"
