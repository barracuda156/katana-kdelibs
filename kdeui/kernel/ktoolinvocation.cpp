/* This file is part of the KDE libraries
    Copyright (C) 2005 Brad Hards <bradh@frogmouth.net>
    Copyright (C) 2006 Thiago Macieira <thiago@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "ktoolinvocation.h"
#include "kdebug.h"
#include "kglobal.h"
#include "kstandarddirs.h"
#include "kcomponentdata.h"
#include "kurl.h"
#include "kservice.h"
#include "klocale.h"
#include "kglobalsettings.h"
#include "kstartupinfo.h"

#include <QtCore/QThread>
#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnectionInterface>
#include <QtGui/QX11Info>

#ifdef Q_WS_X11
#  include <X11/Xlib.h>
#  include <fixx11h.h>
#endif

#define KTOOLINVOCATION_TIMEOUT 250

K_GLOBAL_STATIC(KToolInvocation, kToolInvocation)

KToolInvocation* KToolInvocation::self()
{
    return kToolInvocation;
}

KToolInvocation::KToolInvocation(QObject *parent)
    : QObject(parent),
    klauncherIface(nullptr)
{
    klauncherIface = new QDBusInterface(
        QString::fromLatin1("org.kde.klauncher"),
        QString::fromLatin1("/KLauncher"),
        QString::fromLatin1("org.kde.KLauncher"),
        QDBusConnection::sessionBus(),
        this
    );
    qAddPostRoutine(kToolInvocation.destroy);
}

KToolInvocation::~KToolInvocation()
{
    qRemovePostRoutine(kToolInvocation.destroy);
}

void KToolInvocation::setLaunchEnv(const QString &name, const QString &value)
{
    klauncherIface->asyncCall(QString::fromLatin1("setLaunchEnv"), name, value);
}

bool KToolInvocation::startServiceForUrl(const QString &url, QWidget *window, bool temp)
{
    return startServiceInternal(
        "start_service_by_url", QString(), QStringList() << url, window, temp
    );
}

bool KToolInvocation::startServiceByStorageId(const QString &name, const QStringList &URLs,
                                               QWidget *window, bool temp)
{
    return startServiceInternal("start_service_by_storage_id", name, URLs, window, temp);
}

bool KToolInvocation::startProgram(const QString &name, const QStringList &args, QWidget *window,
                                   bool temp)
{
    return startServiceInternal("start_program", name, args, window, temp);
}

void KToolInvocation::invokeHelp(const QString &anchor,
                                 const QString &_appname)
{
    KUrl url;
    QString appname;
    QString docPath;
    if (_appname.isEmpty()) {
        appname = QCoreApplication::applicationName();
    } else {
        appname = _appname;
    }
    KService::Ptr service(KService::serviceByDesktopName(appname));
    if (service) {
        docPath = service->docPath();
    }
    if (!docPath.isEmpty()) {
        url = KUrl(KUrl(QString::fromLatin1(KDE_HELP_URL)), docPath);
    } else {
        url = QString::fromLatin1(KDE_HELP_URL);
    }
    if (!anchor.isEmpty()) {
        url.addQueryItem(QString::fromLatin1("anchor"), anchor);
    }
    invokeBrowser(url.url());
}

void KToolInvocation::invokeMailer(const QString &address, const QString &subject)
{
    invokeMailer(address, QString(), subject, QString(), QStringList());
}

void KToolInvocation::invokeMailer(const KUrl &mailtoURL, bool allowAttachments)
{
    QString address = mailtoURL.path();
    QString subject;
    QString cc;
    QString body;

    const QStringList queries = mailtoURL.query().mid(1).split(QLatin1Char('&'));
    const QChar comma = QChar::fromLatin1(',');
    QStringList attachURLs;
    for (QStringList::ConstIterator it = queries.begin(); it != queries.end(); ++it)
    {
        QString q = (*it).toLower();
        if (q.startsWith(QLatin1String("subject=")))
            subject = KUrl::fromPercentEncoding((*it).mid(8).toLatin1());
        else
            if (q.startsWith(QLatin1String("cc=")))
                cc = cc.isEmpty()? KUrl::fromPercentEncoding((*it).mid(3).toLatin1()): cc + comma + KUrl::fromPercentEncoding((*it).mid(3).toLatin1());
            else
                if (q.startsWith(QLatin1String("body=")))
                    body = KUrl::fromPercentEncoding((*it).mid(5).toLatin1());
                else
                    if (allowAttachments && q.startsWith(QLatin1String("attach=")))
                        attachURLs.push_back(KUrl::fromPercentEncoding((*it).mid(7).toLatin1()));
                    else
                        if (allowAttachments && q.startsWith(QLatin1String("attachment=")))
                            attachURLs.push_back(KUrl::fromPercentEncoding((*it).mid(11).toLatin1()));
                        else
                            if (q.startsWith(QLatin1String("to=")))
                                address = address.isEmpty()? KUrl::fromPercentEncoding((*it).mid(3).toLatin1()): address + comma + KUrl::fromPercentEncoding((*it).mid(3).toLatin1());
    }

    invokeMailer(address, cc, subject, body, attachURLs);
}

bool KToolInvocation::startServiceInternal(const char *_function,
                                          const QString &name, const QStringList &URLs,
                                          QWidget *window, bool temp, const QString &workdir)
{
    QString function = QString::fromLatin1(_function);
    // make sure there is id, so that user timestamp exists
    QStringList envs;
    if (QX11Info::display()) {
        const QString dpystring = QString::fromLatin1(XDisplayString(QX11Info::display()));
        envs << QLatin1String("DISPLAY=") + dpystring;
    } else {
        const QString dpystring = QString::fromLocal8Bit(qgetenv("DISPLAY"));
        if (!dpystring.isEmpty()) {
            envs << QLatin1String("DISPLAY=") + dpystring;
        }
    }

    QDBusPendingReply<bool> reply;
    if (qstrcmp(_function, "start_service_by_url") == 0) {
        reply = klauncherIface->asyncCall(
            function, URLs.first(), envs, window ? quint64(window->winId()) : 0, temp
        );
    } else if (qstrcmp(_function, "start_program_with_workdir") == 0) {
        reply = klauncherIface->asyncCall(
            function, name, URLs, envs, window ? quint64(window->winId()) : 0, temp, workdir
        );
    } else {
        reply = klauncherIface->asyncCall(
            function, name, URLs, envs, window ? quint64(window->winId()) : 0, temp
        );
    }
    kDebug() << "Waiting for klauncher call to finish" << function;
    while (!reply.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, KTOOLINVOCATION_TIMEOUT);
    }
    kDebug() << "Done waiting for klauncher call to finish" << function;
    if (!reply.isValid()) {
        kError() << "KLauncher error" << reply.error().message();
        return false;
    }
    return reply.value();
}

#include "moc_ktoolinvocation.cpp"
