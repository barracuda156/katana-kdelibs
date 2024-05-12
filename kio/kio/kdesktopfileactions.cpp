/*  This file is part of the KDE libraries
 *  Copyright (C) 1999 Waldo Bastian <bastian@kde.org>
 *                     David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "kdesktopfileactions.h"

#include "krun.h"
#include "kautomount.h"
#include "kmimetype.h"
#include "kmessagebox.h"
#include "kdirnotify.h"
#include "kmountpoint.h"
#include "kstandarddirs.h"
#include "kdesktopfile.h"
#include "kconfiggroup.h"
#include "ktoolinvocation.h"
#include "klocale.h"
#include "kservice.h"
#include "kdebug.h"

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

enum BuiltinServiceType { ST_MOUNT = 0x0E1B05B0, ST_UNMOUNT = 0x0E1B05B1 }; // random numbers

static bool runFSDevice( const KUrl& _url, const KDesktopFile &cfg );
static bool runLink( const KUrl& _url, const KDesktopFile &cfg );

bool KDesktopFileActions::run( const KUrl& u, bool _is_local )
{
    // It might be a security problem to run external untrusted desktop
    // entry files
    if ( !_is_local )
        return false;

    KDesktopFile cfg(u.toLocalFile());
    if ( !cfg.desktopGroup().hasKey("Type") )
    {
        KMessageBox::error(
            nullptr,
            i18n("The desktop entry file %1 has no Type=... entry.", u.toLocalFile())
        );
        return false;
    }

    // kDebug() << "TYPE = " << type.data();

    if ( cfg.hasDeviceType() )
        return runFSDevice( u, cfg );
    else if ( cfg.hasApplicationType()
              || (cfg.readType() == "Service" && !cfg.desktopGroup().readEntry("Exec").isEmpty())) // for kio_settings
        return KToolInvocation::self()->startServiceByStorageId( u.toLocalFile() );
    else if ( cfg.hasLinkType() )
        return runLink( u, cfg );

    KMessageBox::error(
        nullptr,
        i18n("The desktop entry of type\n%1\nis unknown.",  cfg.readType())
    );

    return false;
}

static bool runFSDevice( const KUrl& _url, const KDesktopFile &cfg )
{
    bool retval = false;

    QString dev = cfg.readDevice();

    if ( dev.isEmpty() )
    {
        KMessageBox::error(
            nullptr,
            i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  _url.toLocalFile())
        );
        return retval;
    }

    KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice( dev );
    // Is the device already mounted ?
    if (mp) {
        // Open a new window
        retval = KToolInvocation::self()->startServiceForUrl(mp->mountPoint());
    } else {
        KConfigGroup cg = cfg.desktopGroup();
        bool ro = cg.readEntry("ReadOnly", false);
        QString point = cg.readEntry( "MountPoint" );
        (void) new KAutoMount( ro, dev, point, _url.toLocalFile() );
        retval = false;
    }

    return retval;
}

static bool runLink( const KUrl& _url, const KDesktopFile &cfg )
{
    QString u = cfg.readUrl();
    if ( u.isEmpty() )
    {
        KMessageBox::error(
            nullptr,
            i18n("The desktop entry file\n%1\nis of type Link but has no URL=... entry.",  _url.prettyUrl())
        );
        return false;
    }

    // X-KDE-LastOpenedWith holds the service desktop entry name that
    // should be preferred for opening this URL if possible.
    // This is used by the Recent Documents menu for instance.
    QString lastOpenedWidth = cfg.desktopGroup().readEntry( "X-KDE-LastOpenedWith" );
    if ( !lastOpenedWidth.isEmpty() ) {
        KService::Ptr service = KService::serviceByStorageId(lastOpenedWidth);
        if (!service.isNull()) {
            return KToolInvocation::self()->startServiceByStorageId(service->entryPath(), QStringList() << u, nullptr);
        } else {
            kWarning() << "Last opened with service is not valid" << lastOpenedWidth;
        }
    }
    return KToolInvocation::self()->startServiceForUrl(u, nullptr);
}

QList<KServiceAction> KDesktopFileActions::builtinServices( const KUrl& _url )
{
    QList<KServiceAction> result;

    if ( !_url.isLocalFile() )
        return result;

    bool offerMount = false;
    bool offerUnmount = false;

    KDesktopFile cfg( _url.toLocalFile() );
    if ( cfg.hasDeviceType() ) {  // url to desktop file
        const QString dev = cfg.readDevice();
        if ( dev.isEmpty() ) {
            KMessageBox::error(
                nullptr,
                i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  _url.toLocalFile())
            );
            return result;
        }

        KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice( dev );
        if (mp) {
            offerUnmount = true;
        }
        else {
            offerMount = true;
        }
    }

    if (offerMount) {
        KServiceAction mount("mount", i18n("Mount"), QString(), QString(), false);
        mount.setData(QVariant(ST_MOUNT));
        result.append(mount);
    }

    if (offerUnmount) {
        KServiceAction unmount("unmount", i18n("Unmount"), QString(), QString(), false);
        unmount.setData(QVariant(ST_UNMOUNT));
        result.append(unmount);
    }

    return result;
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices( const QString& path, bool bLocalFiles )
{
    KDesktopFile cfg( path );
    return userDefinedServices( path, cfg, bLocalFiles );
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices( const QString& path, const KDesktopFile& cfg, bool bLocalFiles, const KUrl::List & file_list )
{
    Q_UNUSED(path); // this was just for debugging; we use service.entryPath() now.
    KService service(&cfg);
    return userDefinedServices(service, bLocalFiles, file_list);
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices( const KService& service, bool bLocalFiles, const KUrl::List & file_list )
{
    QList<KServiceAction> result;

    if (!service.isValid()) // e.g. TryExec failed
        return result;

    QStringList keys;
    const QString actionMenu = service.property("X-KDE-GetActionMenu", QVariant::String).toString();
    if (!actionMenu.isEmpty()) {
        const QStringList dbuscall = actionMenu.split(QChar(' '));
        if (dbuscall.count() >= 4) {
            const QString& app       = dbuscall.at( 0 );
            const QString& object    = dbuscall.at( 1 );
            const QString& interface = dbuscall.at( 2 );
            const QString& function  = dbuscall.at( 3 );

            QDBusInterface remote( app, object, interface );
            // Do NOT use QDBus::BlockWithGui here. It runs a nested event loop,
            // in which timers can fire, leading to crashes like #149736.
            QDBusReply<QStringList> reply = remote.call(function, file_list.toStringList());
            keys = reply;               // ensures that the reply was a QStringList
            if (keys.isEmpty())
                return result;
        } else {
            kWarning() << "The desktop file" << service.entryPath()
                       << "has an invalid X-KDE-GetActionMenu entry."
                       << "Syntax is: app object interface function";
        }
    }

    // Now, either keys is empty (all actions) or it's set to the actions we want

    foreach(const KServiceAction& action, service.actions()) {
        if (keys.isEmpty() || keys.contains(action.name())) {
            const QString exec = action.exec();
            if (bLocalFiles || exec.contains("%U") || exec.contains("%u")) {
                result.append( action );
            }
        }
    }

    return result;
}

void KDesktopFileActions::executeService( const KUrl::List& urls, const KServiceAction& action )
{
    // kDebug() << "EXECUTING Service " << action.name();

    int actionData = action.data().toInt();
    if ( actionData == ST_MOUNT || actionData == ST_UNMOUNT ) {
        Q_ASSERT( urls.count() == 1 );
        const QString path = urls.first().toLocalFile();
        // kDebug() << "MOUNT&UNMOUNT";

        KDesktopFile cfg( path );
        if (cfg.hasDeviceType()) { // path to desktop file
            const QString dev = cfg.readDevice();
            if ( dev.isEmpty() ) {
                KMessageBox::error(
                    nullptr,
                    i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  path)
                );
                return;
            }
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice( dev );

            if ( actionData == ST_MOUNT ) {
                // Already mounted? Strange, but who knows ...
                if ( mp ) {
                    kDebug() << "ALREADY Mounted";
                    return;
                }

                const KConfigGroup group = cfg.desktopGroup();
                bool ro = group.readEntry("ReadOnly", false);
                QString point = group.readEntry( "MountPoint" );
                (void)new KAutoMount( ro, dev, point, path, false );
            } else if ( actionData == ST_UNMOUNT ) {
                // Not mounted? Strange, but who knows ...
                if ( !mp )
                    return;

                (void)new KAutoUnmount( mp->mountPoint(), path );
            }
        }
    } else {
        kDebug() << action.name() << "first url's path=" << urls.first().toLocalFile() << "exec=" << action.exec();
        KService actionService(action.text(), action.exec(), action.icon());
        const QStringList urlStrings = urls.toStringList();
        QStringList actionArgs = KRun::processDesktopExec(actionService, urlStrings);
        if (actionArgs.isEmpty()) {
            kWarning() << "empty service command" << action.text() << action.exec();
        } else {
            const QString actionProgram = actionArgs.takeFirst();
            KToolInvocation::self()->startProgram(actionProgram, actionArgs);
        }
        // The action may update the desktop file. Example: eject unmounts (#5129).
        org::kde::KDirNotify::emitFilesChanged(urlStrings);
    }
}

