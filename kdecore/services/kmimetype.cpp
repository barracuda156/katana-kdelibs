/*  This file is part of the KDE libraries
 *  Copyright (C) 1999 Waldo Bastian <bastian@kde.org>
 *                2000-2007 David Faure <faure@kde.org>
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

#include <kglobalsettings.h>

#include "kmimetype.h"
#include "kmimetype_p.h"
#include "kmimetypefactory.h"
#include "kmimetyperepository_p.h"

#include <kdebug.h>
#include <kde_file.h> // KDE::stat
#include <kdeversion.h> // KDE_MAKE_VERSION
#include <klocale.h>
#include <kprotocolinfo.h>
#include <kprotocolinfofactory.h>
#include <kurl.h>
#include <kdesktopfile.h>
#include <kconfiggroup.h>

#include <QFile>
#include <QHash>
#include <QBuffer>
#include <QStack>
#include <QXmlStreamReader>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDirIterator>

extern int servicesDebugArea();

template class KSharedPtr<KMimeType>;

static QString kFolderComment(const KUrl &_url)
{
    if (_url.isEmpty() || !_url.isLocalFile()) {
        return QString();
    }

    KUrl u(_url);
    u.addPath(QString::fromLatin1(".directory"));
    const KDesktopFile cfg(u.toLocalFile());
    return cfg.readComment();
}

static QString kFolderIconName(const KUrl &_url)
{
    if (_url.isEmpty() || !_url.isLocalFile()) {
        return QString();
    }

    // Stating .directory files can cause long freezes when e.g. /home
    // uses autofs for every user's home directory, i.e. opening /home
    // in a file dialog will mount every single home directory.
    // These non-mounted directories can be identified by having 0 size.
    // There are also other directories with 0 size, such as /proc, that may
    // be mounted, but those are unlikely to contain .directory (and checking
    // this would require KMountPoint from kio).
    KDE_struct_stat buff;
    if (KDE_stat(QFile::encodeName(_url.toLocalFile()), &buff) == 0
        && S_ISDIR(buff.st_mode) && buff.st_size == 0) {
        return QString();
    }

    KUrl u(_url);
    u.addPath(QString::fromLatin1(".directory"));

    QString icon;
    // .directory has to be a readable file
    QFileInfo uinfo(u.toLocalFile());
    if (uinfo.isFile() && uinfo.isReadable()) {
        KDesktopFile cfg(u.toLocalFile());
        KConfigGroup group = cfg.desktopGroup();
        icon = group.readEntry("Icon");
        QString empty_icon = group.readEntry("EmptyIcon");

        if (!empty_icon.isEmpty()) {
            bool isempty = true;
            QDirIterator dirIt(_url.toLocalFile(), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
            while (dirIt.hasNext() ) {
                dirIt.next();
                if (dirIt.fileName() != QLatin1String(".directory")) {
                    isempty = false;
                    break;
                }
            }
            if (isempty) {
                return empty_icon;
            }
        }
    }

    if (icon.isEmpty()) {
        return QString();
    }

    if (icon.startsWith(QLatin1String("./"))) {
        // path is relative with respect to the location
        // of the .directory file (#73463)
        KUrl v(_url);
        v.addPath(icon.mid(2));
        icon = v.toLocalFile();
    }

    return icon;
}

static QString kTrashIcon(const QString &emptyIcon)
{
    // need to find if the trash is empty, preferably without using a KIO job. kio_trash leaves an
    // entry in its config file
    KConfig trashConfig(QLatin1String("trashrc"), KConfig::SimpleConfig);
    if (trashConfig.group("Status").readEntry("Empty", true)) {
        return emptyIcon;
    }
    // the default icon for the protocol
    return QString::fromLatin1("user-trash-full");
}


KMimeType::KMimeType(const QString &fullpath, const QString &name)
    : QSharedData(),
    d_ptr(new KMimeTypePrivate(fullpath))
{
    d_ptr->m_strName = name;
}


KMimeType::~KMimeType()
{
    delete d_ptr;
}

KMimeType::Ptr KMimeType::defaultMimeTypePtr()
{
    return KMimeTypeRepository::self()->defaultMimeTypePtr();
}

bool KMimeType::isDefault() const
{
    return name() == defaultMimeType();
}

KMimeType::Ptr KMimeType::mimeType(const QString& name, FindByNameOption options)
{
    return KMimeTypeRepository::self()->findMimeTypeByName(name, options);
}

KMimeType::List KMimeType::allMimeTypes()
{
    // This could be done faster...
    KMimeType::List lst;
    Q_FOREACH(const QString& mimeType, KMimeTypeFactory::self()->allMimeTypes()) {
        if (!mimeType.startsWith(QLatin1String("x-scheme-handler"))) {
            lst.append(KMimeType::mimeType(mimeType));
        }
    }
    return lst;
}

bool KMimeType::isBufferBinaryData(const QByteArray &data)
{
    // Check the first 32 bytes (see shared-mime spec)
    const char* p = data.constData();
    const int end = qMin(32, data.size());
    for (int i = 0; i < end; ++i) {
        if ((unsigned char)(p[i]) < 32 && p[i] != 9 && p[i] != 10 && p[i] != 13) {
            // ASCII control character
            return true;
        }
    }
    return false;
}

KMimeType::Ptr KMimeType::findByUrl(const KUrl &url, mode_t mode,
                                   bool fast_mode, int* accuracy)
{
    KMimeTypeRepository::self()->checkEssentialMimeTypes();

    const bool is_local = url.isLocalFile();
    const QString localfile = url.toLocalFile();
    if (is_local && mode <= 0) {
        KDE_struct_stat buff;
        if (KDE::stat(localfile, &buff) != -1) {
            mode = buff.st_mode;
        }
    }

    if (accuracy) {
        *accuracy = 100;
    }
    if (S_ISDIR(mode)) {
        return KMimeType::mimeType(QLatin1String("inode/directory"));
    } else if (S_ISCHR(mode)) {
        return KMimeType::mimeType(QLatin1String("inode/chardevice"));
    } else if (S_ISBLK(mode)) {
        return KMimeType::mimeType(QLatin1String("inode/blockdevice"));
    } else if (S_ISFIFO(mode)) {
        return KMimeType::mimeType(QLatin1String("inode/fifo"));
    } else if (S_ISSOCK(mode)) {
        return KMimeType::mimeType(QLatin1String("inode/socket"));
    }

    // Try to find out by looking at the filename (if there's one)
    const QString fileName = url.fileName();
    const QString protocol = url.protocol();
    QStringList mimeList;
    if (!fileName.isEmpty() && KProtocolInfo::determineMimetypeFromExtension(protocol)) {
        mimeList = KMimeTypeRepository::self()->findFromFileName(fileName);
        if (mimeList.size() == 1) {
            if (accuracy) {
                *accuracy = 50;
            }
            return KMimeType::mimeType(mimeList.at(0));
        }
    }

    // Try the magic matches
    if (!fast_mode && is_local) {
        QFile file(localfile);
        if (file.open(QIODevice::ReadOnly)) {
            int magicAccuracy = 0;
            // provide enough data for most rules (there are exceptions which require twice as much tho)
            KMimeType::Ptr mime = KMimeTypeRepository::self()->findFromContent(file.read(16384), &magicAccuracy);
            // mime can't be 0, except in case of install problems.
            // However we get magicAccuracy==0 for octet-stream, i.e. no magic match found.
            // kDebug() << "findFromContent said" << (mime?mime->name():QString()) << "with accuracy" << magicAccuracy;
            if (mime && magicAccuracy > 0) {
                // Disambiguate conflicting extensions (if magic found something and the magicrule was <80)
                if (magicAccuracy < 80 && !mimeList.isEmpty()) {
                    // "for glob_match in glob_matches:"
                    // "if glob_match is subclass or equal to sniffed_type, use glob_match"
                    const QString sniffedMime = mime->name();
                    foreach(const QString &m, mimeList) {
                        KMimeType::Ptr mimeFromPattern = KMimeType::mimeType(m);
                        // kDebug() << "sniffedMime=" << sniffedMime << "mimeFromPattern=" << mimeFromPattern->name();
                        if (mimeFromPattern && mimeFromPattern->is(sniffedMime)) {
                            // We have magic + pattern pointing to this, so it's a pretty good match
                            if (accuracy) {
                                *accuracy = 100;
                            }
                            return mimeFromPattern;
                        }
                    }
                }

                if (accuracy) {
                    *accuracy = magicAccuracy;
                }
                return mime;
            }
        }
    }

    KProtocolInfo::Ptr prot = KProtocolInfoFactory::self()->findProtocol(protocol);
    if (prot) {
        const QString protmime = prot->defaultMimeType();
        if (!protmime.isEmpty()) {
            // Find a fallback from the protocol
            if (accuracy) {
                *accuracy = 10;
            }
            return KMimeType::mimeType(protmime);
        }
        // Assume inode/directory if the protocol supports listing and it looks like directory
        const QString path = (is_local ? localfile : url.path());
        if (prot->supportsListing() && (path.endsWith(QLatin1Char('/')) || path.isEmpty())) {
            if (accuracy) {
                *accuracy = 10;
            }
            return KMimeType::mimeType(QLatin1String("inode/directory"));
        }
    }

    if (accuracy) {
        *accuracy = 0;
    }
    return KMimeType::defaultMimeTypePtr();
}

KMimeType::Ptr KMimeType::findByName(const QString &fileName, int *accuracy)
{
    return findByUrl(KUrl(fileName), 0, true, accuracy);
}

KMimeType::Ptr KMimeType::findByContent(const QByteArray &data, int *accuracy)
{
    KMimeTypeRepository::self()->checkEssentialMimeTypes();
    return KMimeTypeRepository::self()->findFromContent(data, accuracy);
}

QString KMimeType::extractKnownExtension(const QString &fileName)
{
    QString pattern;
    KMimeTypeRepository::self()->findFromFileName(fileName, &pattern);
    return pattern;
}

bool KMimeType::isBinaryData(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
         // err, whatever
        return false;
    }
    return isBufferBinaryData(file.read(32));
}

QString KMimeType::iconNameForUrl(const KUrl &url, mode_t mode, const QString &_mimeType)
{
    static const QLatin1String s_trashprotocol = QLatin1String("trash");
    static const QString s_usertrash = QString::fromLatin1("user-trash");

    QString i;
    // root of protocol has priority over the MIME type icon
    if (url.path().length() <= 1) {
        if (url.protocol() == s_trashprotocol) {
            i = kTrashIcon(s_usertrash);
        } else {
            i = KProtocolInfo::icon(url.protocol());
        }
    }
    if (!i.isEmpty()) {
        return i;
    }

    const KMimeType::Ptr mt = (_mimeType.isEmpty() ? findByUrl(url, mode) : KMimeType::mimeType(_mimeType));
    if (!mt) {
        return QString();
    }

    static const QString s_applicationxdestkop = QString::fromLatin1("application/x-desktop");
    if (url.isLocalFile() && mt->is(s_applicationxdestkop)) {
        KDesktopFile cfg(url.toLocalFile());
        i = cfg.readIcon();
        if (cfg.hasLinkType()) {
            const KConfigGroup group = cfg.desktopGroup();
            const QString type = cfg.readPath();
            const QString emptyIcon = group.readEntry("EmptyIcon");
            if (!emptyIcon.isEmpty()) {
                const KUrl cfgurl(cfg.readUrl());
                if (cfgurl.protocol() == s_trashprotocol) {
                    i = kTrashIcon(emptyIcon);
                }
            }
        }
        if (!i.isEmpty()) {
            return i;
        }
    }

    return mt->iconName(url);
}

QString KMimeType::favIconForUrl(const KUrl &url, bool download)
{
    if (url.isLocalFile()
        || !url.protocol().startsWith(QLatin1String("http"))
        || !KMimeTypeRepository::self()->useFavIcons()) {
        return QString();
    }

    QDBusInterface kded(
        QString::fromLatin1("org.kde.kded"),
        QString::fromLatin1("/modules/favicons"),
        QString::fromLatin1("org.kde.FavIcon")
    );
    QDBusReply<QString> iconreply = kded.call(QString::fromLatin1("iconForUrl"), url.url());
    if (iconreply.isValid()) {
        const QString iconfile = iconreply.value();
        if (iconfile.isEmpty() && download) {
            kDebug() << "Downloading icon for" << url.prettyUrl();
            const QDBusReply<void> downloadreply = kded.call(QString::fromLatin1("downloadUrlIcon"), url.url());
            if (!downloadreply.isValid()) {
                kWarning() << "Could not start downloading icon for" << url.prettyUrl();
            }
        }
        return iconfile;
    }
    return QString();
}

bool KMimeTypePrivate::inherits(const QString &mime) const
{
    QStack<QString> toCheck;
    toCheck.push(m_strName);
    while (!toCheck.isEmpty()) {
        const QString current = toCheck.pop();
        if (current == mime) {
            return true;
        }
        Q_FOREACH(const QString& parent, KMimeTypeRepository::self()->parents(current)) {
            toCheck.push(parent);
        }
    }
    return false;
}

bool KMimeType::is(const QString &mimeTypeName) const
{
    Q_D(const KMimeType);
    if (name() == mimeTypeName) {
        return true;
    }
    return d->inherits(KMimeTypeRepository::self()->canonicalName(mimeTypeName));
}

QStringList KMimeType::parentMimeTypes() const
{
    Q_D(const KMimeType);
    return KMimeTypeRepository::self()->parents(d->m_strName);
}

static void collectParentMimeTypes(const QString &mime, QStringList &allParents)
{
    const QStringList parents = KMimeTypeRepository::self()->parents(mime);
    Q_FOREACH(const QString &parent, parents) {
        // I would use QSet, but since order matters I better not
        if (!allParents.contains(parent)) {
            allParents.append(parent);
        }
    }
    // We want a breadth-first search, so that the least-specific parent (octet-stream) is last
    // This means iterating twice, unfortunately.
    Q_FOREACH(const QString &parent, parents) {
        collectParentMimeTypes(parent, allParents);
    }
}

QStringList KMimeType::allParentMimeTypes() const
{
    Q_D(const KMimeType);
    QStringList allParents;
    const QString canonical = KMimeTypeRepository::self()->resolveAlias(name());
    if (!canonical.isEmpty()) {
        allParents.append(canonical);
    }
    collectParentMimeTypes(d->m_strName, allParents);
    return allParents;
}

QString KMimeType::defaultMimeType()
{
    return QString::fromLatin1("application/octet-stream");
}

QString KMimeType::name() const
{
    Q_D(const KMimeType);
    return d->m_strName;
}

QString KMimeType::iconName(const KUrl &url) const
{
    Q_D(const KMimeType);

    if (d->m_strName == QLatin1String("inode/directory")) {
        const QString folderIconName = kFolderIconName(url);
        if (!folderIconName.isEmpty()) {
            return folderIconName;
        }
    }

    d->ensureXmlDataLoaded();
    if (!d->m_iconName.isEmpty()) {
        return d->m_iconName;
    }

    static QHash<QUrl, QString> iconNameCache;
    QString iconNameFromCache = iconNameCache.value(d->m_strName);
    if (!iconNameFromCache.isEmpty()) {
        return iconNameFromCache;
    }
    // Make default icon name from the mimetype name
    // Don't store this in m_iconName, it would make the filetype editor
    // write out icon names in every local mimetype definition file.
    QString icon = d->m_strName;
    const int slashindex = icon.indexOf(QLatin1Char('/'));
    if (slashindex != -1) {
        icon[slashindex] = QLatin1Char('-');
    }
    iconNameCache.insert(d->m_strName, icon);
    return icon;
}

QString KMimeType::comment(const KUrl &url) const
{
    Q_D(const KMimeType);
    if (d->m_strName == QLatin1String("inode/directory")) {
        const QString folderComment = kFolderComment(url);
        if (!folderComment.isEmpty()) {
            return folderComment;
        }
    }
    d->ensureXmlDataLoaded();
    return d->m_strComment;
}

QStringList KMimeType::patterns() const
{
    Q_D(const KMimeType);
    d->ensureXmlDataLoaded();
    return d->m_lstPatterns;
}

// loads comment, icon, mainPattern, m_lstPatterns
void KMimeTypePrivate::ensureXmlDataLoaded() const
{
    if (m_xmlDataLoaded) {
        return;
    }
    m_xmlDataLoaded = true;

    if (m_path.isEmpty()) {
        kWarning() << "No MIME file found for" << m_strName;
        return;
    }

    QFile qfile(m_path);
    if (!qfile.open(QFile::ReadOnly)) {
        return;
    }

    QString comment;
    QString mainPattern;
    const QStringList languageList = KGlobal::locale()->languageList();
    QString preferredLanguage = languageList.first();
    QMap<QString, QString> commentsByLanguage;

    QXmlStreamReader xml(&qfile);
    if (xml.readNextStartElement()) {
        if (xml.name() != "mime-type") {
            return;
        }
        const QString name = xml.attributes().value(QLatin1String("type")).toString();
        if (name.isEmpty()) {
            return;
        }
        if (name.toLower() != m_strName) {
            kWarning() << "Got name" << name << "in file" << m_path << "expected" << m_strName;
        }

        while (xml.readNextStartElement()) {
            const QStringRef tag = xml.name();
            if (tag == "comment") {
                QString lang = xml.attributes().value(QLatin1String("xml:lang")).toString();
                const QString text = xml.readElementText();
                if (lang.isEmpty()) {
                    lang = QLatin1String("en_US");
                }
                if (lang == preferredLanguage) {
                    comment = text;
                } else {
                    commentsByLanguage.insert(lang, text);
                }
                continue; // we called readElementText, so we're at the EndElement already.
            } else if (tag == "icon") { // as written out by shared-mime-info >= 0.40
                m_iconName = xml.attributes().value(QLatin1String("name")).toString();
            } else if (tag == "glob-deleteall") { // as written out by shared-mime-info >= 0.70
                mainPattern.clear();
                m_lstPatterns.clear();
            } else if (tag == "glob") { // as written out by shared-mime-info >= 0.70
                const QString pattern = xml.attributes().value(QLatin1String("pattern")).toString();
                if (mainPattern.isEmpty() && pattern.startsWith(QLatin1Char('*'))) {
                    mainPattern = pattern;
                }
                if (!m_lstPatterns.contains(pattern)) {
                    m_lstPatterns.append(pattern);
                }
            }
            xml.skipCurrentElement();
        }
    }

    if (comment.isEmpty()) {
        Q_FOREACH(const QString &lang, languageList) {
            const QString comm = commentsByLanguage.value(lang);
            if (!comm.isEmpty()) {
                comment = comm;
                break;
            }
            const int pos = lang.indexOf(QLatin1Char('_'));
            if (pos != -1) {
                // "pt_BR" not found? try just "pt"
                const QString shortLang = lang.left(pos);
                const QString comm = commentsByLanguage.value(shortLang);
                if (!comm.isEmpty()) {
                    comment = comm;
                    break;
                }
            }
        }
        if (comment.isEmpty()) {
            kWarning() << "Missing <comment> field in" << m_path;
        }
    }
    m_strComment = comment;

    if (!mainPattern.isEmpty() && m_lstPatterns.first() != mainPattern) {
        // ensure it's first in the list of patterns
        m_lstPatterns.removeAll(mainPattern);
        m_lstPatterns.prepend(mainPattern);
    }
}

QString KMimeType::userSpecifiedIconName() const
{
    Q_D(const KMimeType);
    d->ensureXmlDataLoaded();
    return d->m_iconName;
}

int KMimeType::sharedMimeInfoVersion()
{
    return KMimeTypeRepository::self()->sharedMimeInfoVersion();
}

QString KMimeType::mainExtension() const
{
    Q_FOREACH(const QString &pattern, patterns()) {
        // Skip if if looks like: README or *. or *.*
        // or *.JP*G or *.JP?
        if (pattern.startsWith(QLatin1String("*.")) &&
            pattern.length() > 2 &&
            pattern.indexOf(QLatin1Char('*'), 2) < 0 && pattern.indexOf(QLatin1Char('?'), 2) < 0) {
            return pattern.mid(1);
        }
    }
    // TODO we should also look into the parent mimetype's patterns, no?
    return QString();
}

bool KMimeType::matchFileName(const QString &filename, const QString &pattern)
{
    return KMimeTypeRepository::matchFileName(filename, pattern, Qt::CaseInsensitive);
}
