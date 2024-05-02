/* This file is part of the KDE libraries
   Copyright (C) 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2000- Waldo Bastain <bastain@kde.org>
   Copyright (C) 2000- Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2008 Jarosław Staniek <staniek@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kprotocolmanager.h"
#include "kdeversion.h"
#include "kdebug.h"
#include "kglobal.h"
#include "klocale.h"
#include "kconfiggroup.h"
#include "ksharedconfig.h"
#include "kurl.h"
#include "kprotocolinfofactory.h"
#include "kio/ioslave_defaults.h"
#include "scheduler_p.h"

#include <QCoreApplication>

#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

static KProtocolInfo::Ptr findProtocol(const KUrl &url)
{
    return KProtocolInfoFactory::self()->findProtocol(url.protocol());
}

class KProtocolManagerPrivate
{
public:
    KProtocolManagerPrivate();
    ~KProtocolManagerPrivate();

    KSharedConfig::Ptr config;
};

K_GLOBAL_STATIC(KProtocolManagerPrivate, kProtocolManagerPrivate)

KProtocolManagerPrivate::KProtocolManagerPrivate()
{
    // post routine since KConfig::sync() breaks if called too late
    qAddPostRoutine(kProtocolManagerPrivate.destroy);
}

KProtocolManagerPrivate::~KProtocolManagerPrivate()
{
    qRemovePostRoutine(kProtocolManagerPrivate.destroy);
}

#define PRIVATE_DATA \
KProtocolManagerPrivate *d = kProtocolManagerPrivate

void KProtocolManager::reparseConfiguration()
{
    PRIVATE_DATA;
    if (d->config) {
        d->config->reparseConfiguration();
    }

    // Force slave configs re-read...
    KIO::Scheduler::self()->reparseSlaveConfiguration();
}

KSharedConfig::Ptr KProtocolManager::config()
{
    PRIVATE_DATA;
    if (!d->config) {
        d->config = KSharedConfig::openConfig("kioslaverc", KConfig::NoGlobals);
    }
    return d->config;
}

/*=============================== TIMEOUT SETTINGS ==========================*/
int KProtocolManager::connectTimeout()
{
    KConfigGroup cg( config(), QString() );
    const int value = cg.readEntry("ConnectTimeout", DEFAULT_CONNECT_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, value);
}

int KProtocolManager::responseTimeout()
{
    KConfigGroup cg(config(), QString());
    const int value = cg.readEntry("ResponseTimeout", DEFAULT_RESPONSE_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, value);
}

/*================================= USER-AGENT SETTINGS =====================*/
QString KProtocolManager::defaultUserAgent()
{
    QString useragent;
    QString systemName, systemVersion, machine, supp;
    bool sysInfoFound = false;
    struct utsname unameBuf;
    if (uname(&unameBuf) == 0) {
        sysInfoFound = true;
        useragent += unameBuf.sysname;

        useragent += QL1C(' ');
        useragent += unameBuf.release;

        useragent += QL1C(' ');
        useragent += unameBuf.machine;
    }

    if (sysInfoFound) {
        useragent += QL1S("; ");
    }
    useragent += QL1S("Katana ");
    useragent += KDE::versionString();

    // kDebug() << "USERAGENT STRING:" << useragent;
    return useragent;
}

QString KProtocolManager::acceptLanguagesHeader()
{
    static const QString english = QString::fromLatin1("en");

    // User's desktop language preference.
    QStringList languageList = KGlobal::locale()->languageList();

    // Replace possible "C" in the language list with "en", unless "en" is
    // already pressent. This is to keep user's priorities in order.
    // If afterwards "en" is still not present, append it.
    int idx = languageList.indexOf(QString::fromLatin1("C"));
    if (idx != -1) {
        if (languageList.contains(english)) {
            languageList.removeAt(idx);
        } else {
            languageList[idx] = english;
        }
    }
    if (!languageList.contains(english)) {
        languageList += english;
    }

    // The header is composed of comma separated languages, with an optional
    // associated priority estimate (q=1..0) defaulting to 1.
    // As our language tags are already sorted by priority, we'll just decrease
    // the value evenly
    int prio = 10;
    QString header;
    Q_FOREACH (const QString &lang,languageList) {
        header += lang;
        if (prio < 10) {
            header += QL1S(";q=0.");
            header += QString::number(prio);
        }
        // do not add cosmetic whitespace in here : it is less compatible (#220677)
        header += QL1S(",");
        if (prio > 1) {
            --prio;
        }
    }
    header.chop(1);

    // Some of the languages may have country specifier delimited by
    // underscore, or modifier delimited by at-sign.
    // The header should use dashes instead.
    header.replace('_', '-');
    header.replace('@', '-');

    return header;
}

/*==================================== OTHERS ===============================*/

bool KProtocolManager::markPartial()
{
    return config()->group(QByteArray()).readEntry("MarkPartial", true);
}

int KProtocolManager::minimumKeepSize()
{
    return config()->group(QByteArray()).readEntry("MinimumKeepSize", DEFAULT_MINIMUM_KEEP_SIZE);
}

bool KProtocolManager::autoResume()
{
    return config()->group(QByteArray()).readEntry("AutoResume", true);
}

/* =========================== PROTOCOL CAPABILITIES ============== */
bool KProtocolManager::isSourceProtocol(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_isSourceProtocol;
}

bool KProtocolManager::supportsListing(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsListing;
}

bool KProtocolManager::supportsReading(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsReading;
}

bool KProtocolManager::supportsWriting(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsWriting;
}

bool KProtocolManager::supportsMakeDir(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsMakeDir;
}

bool KProtocolManager::supportsDeleting(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsDeleting;
}

bool KProtocolManager::supportsLinking(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsLinking;
}

bool KProtocolManager::supportsMoving(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_supportsMoving;
}

bool KProtocolManager::canCopyFromFile(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_canCopyFromFile;
}

bool KProtocolManager::canCopyToFile(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->m_canCopyToFile;
}

bool KProtocolManager::canRenameFromFile(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->canRenameFromFile();
}

bool KProtocolManager::canRenameToFile(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->canRenameToFile();
}

bool KProtocolManager::canDeleteRecursive(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return false;
    }
    return prot->canDeleteRecursive();
}

KProtocolInfo::FileNameUsedForCopying KProtocolManager::fileNameUsedForCopying(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return KProtocolInfo::FromUrl;
    }
    return prot->fileNameUsedForCopying();
}

QString KProtocolManager::defaultMimetype(const KUrl &url)
{
    KProtocolInfo::Ptr prot = findProtocol(url);
    if (!prot) {
        return QString();
    }
    return prot->m_defaultMimetype;
}

QString KProtocolManager::charsetFor(const KUrl &url)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(KProtocolInfo::config(url.scheme()), KConfig::NoGlobals);
    if (config) {
        return config->group(url.host()).readEntry(QLatin1String("Charset"));
    }
    return QString();
}

#undef PRIVATE_DATA
