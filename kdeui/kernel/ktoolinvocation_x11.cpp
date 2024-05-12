/* This file is part of the KDE libraries
    Copyright (c) 1997,1998 Matthias Kalle Dalheimer <kalle@kde.org>
    Copyright (c) 1999 Espen Sand <espen@kde.org>
    Copyright (c) 2000-2004 Frerich Raabe <raabe@kde.org>
    Copyright (c) 2003,2004 Oswald Buddenhagen <ossi@kde.org>
    Copyright (c) 2006 Thiago Macieira <thiago@kde.org>
    Copyright (C) 2008 Aaron Seigo <aseigo@kde.org>

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
#include "kconfig.h"
#include "kglobal.h"
#include "kshell.h"
#include "kservice.h"
#include "kconfiggroup.h"
#include "kmimetypetrader.h"
#include "kurl.h"
#include "kdebug.h"

void KToolInvocation::invokeMailer(const QString &address)
{
    KConfig config(QString::fromLatin1("emaildefaults"));
    KConfigGroup profileGrp(&config, "General");

    QString command = profileGrp.readPathEntry("EmailClient", QString::fromLatin1("kmail"));
    if( !command.contains( QLatin1Char('%') ))
    {
        command += QLatin1String(" %u");
    }

    if (profileGrp.readEntry("TerminalClient", false))
    {
        KConfigGroup confGroup( KGlobal::config(), "General" );
        QString preferredTerminal = confGroup.readPathEntry("TerminalApplication", QString::fromLatin1("konsole"));
        command = preferredTerminal + QString::fromLatin1(" -e ") + command;
    }

    QStringList cmdTokens = KShell::splitArgs(command);
    QString cmd = cmdTokens.takeFirst();
    startProgram(cmd, cmdTokens);
}

void KToolInvocation::invokeBrowser(const QString &url)
{
    // This method should launch a webbrowser, preferably without doing a mimetype
    // check first like kde-open would do.
    const KService::Ptr htmlApp = KMimeTypeTrader::self()->preferredService(QLatin1String("text/html"));
    if (htmlApp) {
        startServiceByStorageId(htmlApp->entryPath(), QStringList() << url);
        return;
    }

    // if one cannot be found then launch the service for the URL MIME type
    startServiceForUrl(url);
}

void KToolInvocation::invokeTerminal(const QString &command,
                                     const QString &workdir)
{
    KConfigGroup confGroup( KGlobal::config(), "General" );
    QString exec = confGroup.readPathEntry("TerminalApplication", QString::fromLatin1("konsole"));
    QStringList cmdTokens = KShell::splitArgs(exec);
    if (!command.isEmpty()) {
        if (exec == QLatin1String("konsole")) {
            cmdTokens << QString::fromLatin1("--noclose");
        } else if (exec.startsWith(QLatin1String("xterm"))) {
            cmdTokens << QString::fromLatin1("-hold");
        }

        cmdTokens << QString::fromLatin1("-e") << command;
    }


    QString cmd = cmdTokens.takeFirst();

    startServiceInternal(
        "start_program_with_workdir", cmd, cmdTokens, nullptr, false, workdir
    );
}
