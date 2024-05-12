/*  This file is part of the KDE libraries
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "klauncher.h"
#include "klauncher_adaptor.h"
#include "kaboutdata.h"
#include "kdeversion.h"
#include "kapplication.h"
#include "kcmdlineargs.h"
#include "kdebug.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>

KLauncher::KLauncher(QObject *parent)
    : QObject(parent)
{
    m_adaptor = new KLauncherAdaptor(this);

    QDBusConnection session = QDBusConnection::sessionBus();
    session.registerObject("/KLauncher", this);
    session.registerService("org.kde.klauncher");
}

KLauncher::~KLauncher()
{
    QDBusConnection session = QDBusConnection::sessionBus();
    session.unregisterObject("/KLauncher");
    session.unregisterService("org.kde.klauncher");
}

int main(int argc, char *argv[])
{
    KAboutData aboutData(
        "klauncher", "kdelibs4", ki18n("KDE launcher"),
        KDE_VERSION_STRING,
        ki18n("KDE launcher - launches and autostarts applications")
    );
    aboutData.setProgramIconName("system-run");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KApplication app;
    app.setQuitOnLastWindowClosed(false);
    
    QDBusConnection session = QDBusConnection::sessionBus();
    if (!session.isConnected()) {
        kWarning() << "No DBUS session-bus found. Check if you have started the DBUS server.";
        return 1;
    }
    QDBusReply<bool> sessionReply = session.interface()->isServiceRegistered("org.kde.klauncher");
    if (sessionReply.isValid() && sessionReply.value() == true) {
        kWarning() << "Another instance of klauncher is already running!";
        return 2;
    }

    KLauncher klauncher(&app);
    return app.exec(); // keep running
}

#include "moc_klauncher.cpp"
