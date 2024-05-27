/* This file is part of the KDE project
   Copyright (C) 1999-2006 David Faure <faure@kde.org>
   2001 Carsten Pfeiffer <pfeiffer@kde.org>

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

#include "kfileitem.h"

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtCore/QFile>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

#include <kdebug.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmimetype.h>
#include <krun.h>
#include <kde_file.h>
#include <kdesktopfile.h>
#include <kmountpoint.h>
#include <kconfiggroup.h>
#include <kuser.h>
#include <ktoolinvocation.h>

static bool isKDirShare(const QString &dirpath)
{
    static QDBusInterface kdirshareiface(
        QString::fromLatin1("org.kde.kded"),
        QString::fromLatin1("/modules/kdirshare"),
        QString::fromLatin1("org.kde.kdirshare")
    );
    QDBusReply<bool> kdirsharereply = kdirshareiface.call("isShared", dirpath);
    if (!kdirsharereply.isValid()) {
        return false;
    }
    return kdirsharereply.value();
}

// avoid creating these QStrings again and again
static const QLatin1String s_dot = QLatin1String(".");

class KFileItemPrivate : public QSharedData
{
public:
    KFileItemPrivate(const KIO::UDSEntry &entry,
                     const KUrl &itemOrDirUrl,
                     bool urlIsDirectory)
        : m_entry(entry),
          m_url(itemOrDirUrl),
          m_pMimeType(nullptr),
          m_fileMode(KFileItem::Unknown),
          m_permissions(KFileItem::Unknown),
          m_bMarked(false),
          m_bLink(false),
          m_bIsLocalUrl(itemOrDirUrl.isLocalFile())
    {
        if (entry.count() != 0) {
            // extract fields from the KIO::UDS Entry

            m_fileMode = m_entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
            m_permissions = m_entry.numberValue(KIO::UDSEntry::UDS_ACCESS);
            m_strName = m_entry.stringValue(KIO::UDSEntry::UDS_NAME);

            const QString displayName = m_entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (!displayName.isEmpty()) {
                m_strText = displayName;
            } else {
                m_strText = KIO::decodeFileName(m_strName);
            }

            const QString urlStr = m_entry.stringValue(KIO::UDSEntry::UDS_URL);
            const bool UDS_URL_seen = !urlStr.isEmpty();
            if (UDS_URL_seen) {
                m_url = KUrl(urlStr);
                if (m_url.isLocalFile()) {
                    m_bIsLocalUrl = true;
                }
            }
            const QString mimeTypeStr = m_entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
            if (!mimeTypeStr.isEmpty()) {
                m_pMimeType = KMimeType::mimeType(mimeTypeStr);
            }

            m_guessedMimeType = m_entry.stringValue(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE);
            m_bLink = !m_entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST).isEmpty(); // we don't store the link dest

            if (urlIsDirectory && !UDS_URL_seen && !m_strName.isEmpty() && m_strName != s_dot) {
                m_url.addPath(m_strName);
            }
        } else {
            Q_ASSERT(!urlIsDirectory);
            m_strName = itemOrDirUrl.fileName();
            m_strText = KIO::decodeFileName(m_strName);
        }
        init();
    }

    /**
     * Computes the text and mode from the UDSEntry
     * Called by constructor, but can be called again later
     * Nothing does that anymore though (I guess some old KonqFileItem did)
     * so it's not a protected method of the public class anymore.
     */
    void init();

    QDateTime time(KFileItem::FileTimes which) const;
    void setTime(KFileItem::FileTimes which, long long time_t_val) const;
    QString user() const;
    QString group() const;

    /**
     * The UDSEntry that contains the data for this fileitem, if it came from a directory listing.
     */
    mutable KIO::UDSEntry m_entry;
    /**
     * The url of the file
     */
    KUrl m_url;

    /**
     * The text for this item, i.e. the file name without path,
     */
    QString m_strName;

    /**
     * The text for this item, i.e. the file name without path, decoded
     * ('%%' becomes '%', '%2F' becomes '/')
     */
    QString m_strText;

    /**
     * The icon name for this item.
     */
    mutable QString m_iconName;

    /**
     * The filename in lower case (to speed up sorting)
     */
    mutable QString m_strLowerCaseName;

    /**
     * The mimetype of the file
     */
    mutable KMimeType::Ptr m_pMimeType;

    /**
     * The file mode
     */
    mode_t m_fileMode;
    /**
     * The permissions
     */
    mode_t m_permissions;

    /**
     * Marked : see mark()
     */
    bool m_bMarked;
    /**
     * Whether the file is a link
     */
    bool m_bLink;
    /**
     * True if local file
     */
    bool m_bIsLocalUrl;

    // For special case like link to dirs over FTP
    QString m_guessedMimeType;

    mutable QDateTime m_time[3];
};

void KFileItemPrivate::init()
{
    // determine mode and/or permissions if unknown
    // TODO: delay this until requested
    if (m_fileMode == KFileItem::Unknown || m_permissions == KFileItem::Unknown) {
        mode_t mode = 0;
        if (m_url.isLocalFile()) {
            /* directories may not have a slash at the end if
             * we want to stat() them; it requires that we
             * change into it .. which may not be allowed
             * stat("/is/unaccessible")  -> rwx------
             * stat("/is/unaccessible/") -> EPERM            H.Z.
             * This is the reason for the -1
             */
            KDE_struct_stat buf;
            const QString path = m_url.toLocalFile(KUrl::RemoveTrailingSlash);
            if (KDE::lstat(path, &buf) == 0) {
                mode = buf.st_mode;
                if (S_ISLNK(mode)) {
                    m_bLink = true;
                    if (KDE::stat( path, &buf) == 0) {
                        mode = buf.st_mode;
                    } else {
                        // link pointing to nowhere (see kio/file/file.cc)
                        mode = (S_IFMT-1) | S_IRWXU | S_IRWXG | S_IRWXO;
                    }
                }
                // While we're at it, store the times
                setTime(KFileItem::ModificationTime, buf.st_mtime);
                setTime(KFileItem::AccessTime, buf.st_atime);
                if (m_fileMode == KFileItem::Unknown)
                    // extract file type
                    m_fileMode = mode & S_IFMT;
                if (m_permissions == KFileItem::Unknown) {
                    // extract permissions
                    m_permissions = mode & 07777;
                }
            } else {
                kDebug() << path << "does not exist anymore";
            }
        }
    }
}

void KFileItemPrivate::setTime(KFileItem::FileTimes mappedWhich, long long time_t_val) const
{
    m_time[mappedWhich].setTime_t(time_t_val);
    m_time[mappedWhich] = m_time[mappedWhich].toLocalTime(); // #160979
}

QDateTime KFileItemPrivate::time(KFileItem::FileTimes mappedWhich) const
{
    if (!m_time[mappedWhich].isNull()) {
        return m_time[mappedWhich];
    }

    // Extract it from the KIO::UDSEntry
    long long fieldVal = -1;
    switch (mappedWhich) {
        case KFileItem::ModificationTime: {
            fieldVal = m_entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
            break;
        }
        case KFileItem::AccessTime: {
            fieldVal = m_entry.numberValue(KIO::UDSEntry::UDS_ACCESS_TIME, -1);
            break;
        }
        case KFileItem::CreationTime: {
            fieldVal = m_entry.numberValue(KIO::UDSEntry::UDS_CREATION_TIME, -1);
            break;
        }
    }
    if (fieldVal != -1) {
        setTime(mappedWhich, fieldVal);
        return m_time[mappedWhich];
    }

    // If not in the KIO::UDSEntry, or if UDSEntry empty, use stat() [if local URL]
    if (m_bIsLocalUrl) {
        KDE_struct_stat buf;
        if (KDE::stat(m_url.toLocalFile(KUrl::RemoveTrailingSlash), &buf) == 0) {
            setTime(KFileItem::ModificationTime, buf.st_mtime);
            setTime(KFileItem::AccessTime, buf.st_atime);
            m_time[KFileItem::CreationTime] = QDateTime();
            return m_time[mappedWhich];
        }
    }
    return QDateTime();
}

///////

KFileItem::KFileItem()
    : d(nullptr)
{
}

KFileItem::KFileItem(const KIO::UDSEntry& entry, const KUrl &dirUrl)
    : d(new KFileItemPrivate(entry, dirUrl, true))
{
}

KFileItem::KFileItem(const KUrl &url)
    : d(new KFileItemPrivate(KIO::UDSEntry(), url, false))
{
}


KFileItem::KFileItem(const KFileItem &other)
    : d(other.d)
{
}

KFileItem::~KFileItem()
{
}

void KFileItem::refresh()
{
    if (!d) {
        kWarning() << "null item";
        return;
    }

    d->m_fileMode = KFileItem::Unknown;
    d->m_permissions = KFileItem::Unknown;
    refreshMimeType();

    // basically, can't trust any information while listing. everything could have changed.
    // clearing m_entry makes it possible to detect changes in the size of the file, the time
    // information, etc.
    d->m_entry.clear();
    d->init();
}

void KFileItem::refreshMimeType()
{
    if (!d) {
        return;
    }

    d->m_pMimeType = nullptr;
    d->m_iconName.clear();
}

void KFileItem::setUrl(const KUrl &url)
{
    if (!d) {
        kWarning() << "null item";
        return;
    }

    d->m_url = url;
    setName(url.fileName());
}

void KFileItem::setName(const QString &name)
{
    if (!d) {
        kWarning() << "null item";
        return;
    }

    d->m_strName = name;
    d->m_strText = KIO::decodeFileName(d->m_strName);
    if (d->m_entry.contains(KIO::UDSEntry::UDS_NAME)) {
        d->m_entry.insert(KIO::UDSEntry::UDS_NAME, d->m_strName); // #195385
    }
}

QString KFileItem::linkDest() const
{
    if (!d) {
        return QString();
    }

    // Extract it from the KIO::UDSEntry
    const QString linkStr = d->m_entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);
    if (!linkStr.isEmpty()) {
        return linkStr;
    }

    // If not in the KIO::UDSEntry or if UDSEntry empty use readlink() [if local URL]
    if (d->m_bIsLocalUrl) {
        char buf[1000];
        int n = readlink(QFile::encodeName(d->m_url.toLocalFile(KUrl::RemoveTrailingSlash)), buf, sizeof(buf) - 1);
        if (n != -1) {
            buf[n] = 0;
            return QFile::decodeName(buf);
        }
    }
    return QString();
}

QString KFileItem::localPath() const
{
    if (!d) {
        return QString();
    }
    if (d->m_bIsLocalUrl) {
        return d->m_url.toLocalFile();
    }
    return QString();
}

KIO::filesize_t KFileItem::size() const
{
    if (!d) {
        return 0;
    }

    // Extract it from the KIO::UDSEntry
    long long fieldVal = d->m_entry.numberValue(KIO::UDSEntry::UDS_SIZE, -1);
    if (fieldVal != -1) {
        return fieldVal;
    }

    // If not in the KIO::UDSEntry, or if UDSEntry empty, use stat() [if local URL]
    if (d->m_bIsLocalUrl) {
        KDE_struct_stat buf;
        if (KDE::stat(d->m_url.toLocalFile(KUrl::RemoveTrailingSlash), &buf) == 0) {
            return buf.st_size;
        }
    }
    return 0;
}

bool KFileItem::hasExtendedACL() const
{
    if (!d) {
        return false;
    }
    // Check if the field exists; its value doesn't matter
    return d->m_entry.contains(KIO::UDSEntry::UDS_EXTENDED_ACL);
}

KACL KFileItem::ACL() const
{
    if (!d) {
        return KACL();
    }
    if (hasExtendedACL()) {
        // Extract it from the KIO::UDSEntry
        const QString fieldVal = d->m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING);
        if (!fieldVal.isEmpty()) {
            return KACL(fieldVal);
        }
    }
    // create one from the basic permissions
    return KACL( d->m_permissions );
}

KACL KFileItem::defaultACL() const
{
    if (!d) {
        return KACL();
    }
    // Extract it from the KIO::UDSEntry
    const QString fieldVal = d->m_entry.stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING);
    if (!fieldVal.isEmpty()) {
        return KACL(fieldVal);
    }
    return KACL();
}

QDateTime KFileItem::time(FileTimes which) const
{
    if (!d) {
        return QDateTime();
    }
    return d->time(which);
}


QString KFileItem::user() const
{
    if (!d) {
        return QString();
    }
    return d->user();
}

QString KFileItemPrivate::user() const
{
    QString userName = m_entry.stringValue(KIO::UDSEntry::UDS_USER);
    if (userName.isEmpty() && m_bIsLocalUrl) {
        KDE_struct_stat buff;
        // get uid/gid of the link, if it's a link
        if (KDE::lstat(m_url.toLocalFile(KUrl::RemoveTrailingSlash), &buff) == 0) {
            const KUser kuser(buff.st_uid);
            if (kuser.isValid()) {
                userName = kuser.loginName();
                m_entry.insert(KIO::UDSEntry::UDS_USER, userName);
            }
        }
    }
    return userName;
}

QString KFileItem::group() const
{
    if (!d) {
        return QString();
    }
    return d->group();
}

QString KFileItemPrivate::group() const
{
    QString groupName = m_entry.stringValue(KIO::UDSEntry::UDS_GROUP);
    if (groupName.isEmpty() && m_bIsLocalUrl) {
        KDE_struct_stat buff;
        // get uid/gid of the link, if it's a link
        if (KDE::lstat(m_url.toLocalFile(KUrl::RemoveTrailingSlash), &buff) == 0) {
            const KUserGroup kusergroup(buff.st_gid);
            if (kusergroup.isValid()) {
                groupName = kusergroup.name();
            }
            if (groupName.isEmpty()) {
                groupName = QString::number(buff.st_gid);
            }
            m_entry.insert(KIO::UDSEntry::UDS_GROUP, groupName);
        }
    }
    return groupName;
}

QString KFileItem::mimetype() const
{
    if (!d) {
        return QString();
    }
    KMimeType::Ptr mime = mimeTypePtr();
    if (!mime) {
        return QString();
    }
    return mime->name();
}

QString KFileItem::mimeComment() const
{
    if (!d) {
        return QString();
    }

    const QString displayType = d->m_entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_TYPE);
    if (!displayType.isEmpty()) {
        return displayType;
    }

    KMimeType::Ptr mime = mimeTypePtr();
    // This cannot move to kio_file (with UDS_DISPLAY_TYPE) because it needs
    // the mimetype to be determined, which is done here, and possibly delayed...
    if (d->m_bIsLocalUrl && mime->is("application/x-desktop")) {
        KDesktopFile cfg(d->m_url.toLocalFile());
        QString comment = cfg.desktopGroup().readEntry("Comment");
        if (!comment.isEmpty()) {
            return comment;
        }
    }

    QString comment = mime->comment(d->m_url);
    // kDebug() << "finding comment for " << d->m_url.url() << " : " << d->m_pMimeType->name();
    if (!comment.isEmpty()) {
        return comment;
    }
    return mime->name();
}

QString KFileItem::iconName() const
{
    if (!d) {
        return QString();
    }

    if (!d->m_iconName.isEmpty()) {
        return d->m_iconName;
    }

    d->m_iconName = d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
    if (!d->m_iconName.isEmpty()) {
        return d->m_iconName;
    }

    KMimeType::Ptr mime;
    // Use guessed mimetype for the icon
    if (!d->m_guessedMimeType.isEmpty()) {
        mime = KMimeType::mimeType(d->m_guessedMimeType);
    } else {
        mime = mimeTypePtr();
    }

    if (d->m_bIsLocalUrl && isDesktopFile()) {
        KDesktopFile cfg(d->m_url.toLocalFile());
        d->m_iconName = cfg.readIcon();
        if (cfg.hasLinkType()) {
            const KConfigGroup group = cfg.desktopGroup();
            const QString type = cfg.readPath();
            const QString emptyIcon = group.readEntry("EmptyIcon");
            if (!emptyIcon.isEmpty()) {
                const QString u = cfg.readUrl();
                const KUrl url(u);
                if (url.protocol() == "trash") {
                    // We need to find if the trash is empty, preferably  without using a KIO job.
                    // So instead kio_trash leaves an entry in its config file for us.
                    KConfig trashConfig("trashrc", KConfig::SimpleConfig);
                    if (trashConfig.group("Status").readEntry("Empty", true)) {
                        d->m_iconName = emptyIcon;
                    }
                }
            }
        }
        if (!d->m_iconName.isEmpty()) {
            return d->m_iconName;
        }
    }

    // kDebug() << "finding icon for" << d->m_url << ":" << d->m_iconName;
    d->m_iconName = mime->iconName(d->m_url);
    return d->m_iconName;
}

QStringList KFileItem::overlays() const
{
    if (!d) {
        return QStringList();
    }

    QStringList names = d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_OVERLAY_NAMES).split(',');
    if (d->m_bLink) {
        names.append("emblem-symbolic-link");
    }

    // Locked dirs have a special icon, use the overlay for files only
    if (!S_ISDIR(d->m_fileMode) && !isReadable()) {
        names.append("object-locked");
    }

    if (d->m_bIsLocalUrl && isDesktopFile()) {
        KDesktopFile cfg(localPath());
        const KConfigGroup group = cfg.desktopGroup();

        // Add a warning emblem if this is an executable desktop file
        // which is untrusted.
        if (group.hasKey("Exec") && !KDesktopFile::isAuthorizedDesktopFile(localPath())) {
            names.append("emblem-important");
        }

        if (cfg.hasDeviceType()) {
            const QString dev = cfg.readDevice();
            if (!dev.isEmpty()) {
                KMountPoint::Ptr mountPoint = KMountPoint::currentMountPoints().findByDevice(dev);
                if (mountPoint) {
                    names.append("emblem-mounted");
                }
            }
        }
    }

    if (isHidden()) {
        names.append("hidden");
    }

    if (S_ISDIR(d->m_fileMode) && d->m_bIsLocalUrl) {
        if (isKDirShare(d->m_url.toLocalFile())) {
            // kDebug() << d->m_url.path();
            names.append("network-workgroup");
        }
    }

    if (d->m_pMimeType && d->m_url.fileName().endsWith( QLatin1String(".gz")) &&
        d->m_pMimeType->is("application/x-gzip")) {
        names.append("application-zip");
    }

    return names;
}

QString KFileItem::comment() const
{
    if (!d) {
        return QString();
    }
    return d->m_entry.stringValue(KIO::UDSEntry::UDS_COMMENT);
}

// TODO: where is this used?
QPixmap KFileItem::pixmap(int _size, int _state) const
{
    if (!d) {
        return QPixmap();
    }

    const QString iconName = d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
    if (!iconName.isEmpty()) {
        return DesktopIcon(iconName, _size, _state);
    }

    if (!d->m_pMimeType) {
        // No mimetype determined yet, go for a fast default icon
        if (S_ISDIR(d->m_fileMode)) {
            const KMimeType::Ptr mimeType = KMimeType::mimeType("inode/directory");
            if (mimeType) {
                return DesktopIcon(mimeType->iconName(), _size, _state);
            } else {
                kWarning() << "No mimetype for inode/directory could be found. Check your installation.";
            }
        }
        return DesktopIcon("unknown", _size, _state);
    }

    KMimeType::Ptr mime;
    // Use guessed mimetype for the icon
    if (!d->m_guessedMimeType.isEmpty()) {
        mime = KMimeType::mimeType(d->m_guessedMimeType);
    } else {
        mime = d->m_pMimeType;
    }

    // Support for gzipped files: extract mimetype of contained file
    // See also the relevant code in overlays, which adds the zip overlay.
    if (mime->name() == "application/x-gzip" && d->m_url.fileName().endsWith( QLatin1String(".gz"))) {
        KUrl sf;
        sf.setPath(d->m_url.path().left( d->m_url.path().length() - 3));
        // kDebug() << "subFileName=" << subFileName;
        mime = KMimeType::findByUrl(sf, 0, !d->m_bIsLocalUrl);
    }

    QPixmap p = KIconLoader::global()->loadMimeTypeIcon(mime->iconName(d->m_url), KIconLoader::Desktop, _size, _state);
    // kDebug() << "finding pixmap for " << d->m_url.url() << " : " << mime->name();
    if (p.isNull()) {
        kWarning() << "Pixmap not found for mimetype " << d->m_pMimeType->name();
    }
    return p;
}

bool KFileItem::isReadable() const
{
    if (!d) {
        return false;
    }

    if (d->m_permissions != KFileItem::Unknown) {
        // No read permission at all
        if (!(S_IRUSR & d->m_permissions) && !(S_IRGRP & d->m_permissions) && !(S_IROTH & d->m_permissions)) {
            return false;
        }
    }

    // Or if we can't read it [using ::access()] - not network transparent
    if (d->m_bIsLocalUrl && KDE::access(d->m_url.toLocalFile(), R_OK) == -1) {
        return false;
    }
    return true;
}

bool KFileItem::isWritable() const
{
    if (!d) {
        return false;
    }

    if (d->m_permissions != KFileItem::Unknown) {
        // No write permission at all
        if (!(S_IWUSR & d->m_permissions) && !(S_IWGRP & d->m_permissions) && !(S_IWOTH & d->m_permissions)) {
            return false;
        }
    }

    // Or if we can't read it [using ::access()] - not network transparent
    if (d->m_bIsLocalUrl && KDE::access(d->m_url.toLocalFile(), W_OK) == -1) {
        return false;
    }

    return true;
}

bool KFileItem::isHidden() const
{
    if (!d) {
        return false;
    }
    // Prefer the filename that is part of the URL, in case the display name is different.
    QString fileName = d->m_url.fileName();
    if (fileName.isEmpty()) {
        // e.g. "trash:/"
        fileName = d->m_strName;
    }
    return (fileName.length() > 1 && fileName[0] == '.');  // Just "." is current directory, not hidden.
}

bool KFileItem::isDir() const
{
    if (!d) {
        return false;
    }
    if (d->m_fileMode == KFileItem::Unknown) {
        // Probably the file was deleted already, and KDirLister hasn't told the world yet.
        //kDebug() << d << url() << "can't say -> false";
        return false; // can't say for sure, so no
    }
    return (S_ISDIR(d->m_fileMode));
}

bool KFileItem::isFile() const
{
    if (!d) {
        return false;
    }
    return !isDir();
}


QString KFileItem::getStatusBarInfo() const
{
    if (!d) {
        return QString();
    }

    QString text = d->m_strText;
    const QString comment = mimeComment();
    if (d->m_bLink) {
        text += ' ';
        if (comment.isEmpty()) {
            text += i18n( "(Symbolic Link to %1)", linkDest());
        } else {
            text += i18n("(%1, Link to %2)", comment, linkDest());
        }
    } else if (targetUrl() != url()) {
        text += i18n(" (Points to %1)", targetUrl().pathOrUrl());
    } else if (S_ISREG(d->m_fileMode)) {
        text += QString(" (%1, %2)").arg(comment, KIO::convertSize(size()));
    } else {
        text += QString(" (%1)").arg(comment);
    }
    return text;
}


void KFileItem::run(QWidget *parentWidget) const
{
    if (!d) {
        kWarning() << "null item";
        return;
    }
    KToolInvocation::self()->startServiceForUrl(targetUrl().url(), parentWidget);
}

bool KFileItem::cmp(const KFileItem &item) const
{
    if (!d && !item.d) {
        return true;
    }
    if (!d || !item.d) {
        return false;
    }

#if 0
    kDebug() << "Comparing" << d->m_url << "and" << item.d->m_url;
    kDebug() << " name" << (d->m_strName == item.d->m_strName);
    kDebug() << " local" << (d->m_bIsLocalUrl == item.d->m_bIsLocalUrl);
    kDebug() << " mode" << (d->m_fileMode == item.d->m_fileMode);
    kDebug() << " perm" << (d->m_permissions == item.d->m_permissions);
    kDebug() << " UDS_USER" << (d->user() == item.d->user());
    kDebug() << " UDS_GROUP" << (d->group() == item.d->group());
    kDebug() << " UDS_EXTENDED_ACL" << (d->m_entry.stringValue( KIO::UDSEntry::UDS_EXTENDED_ACL ) == item.d->m_entry.stringValue( KIO::UDSEntry::UDS_EXTENDED_ACL ));
    kDebug() << " UDS_ACL_STRING" << (d->m_entry.stringValue( KIO::UDSEntry::UDS_ACL_STRING ) == item.d->m_entry.stringValue( KIO::UDSEntry::UDS_ACL_STRING ));
    kDebug() << " UDS_DEFAULT_ACL_STRING" << (d->m_entry.stringValue( KIO::UDSEntry::UDS_DEFAULT_ACL_STRING ) == item.d->m_entry.stringValue( KIO::UDSEntry::UDS_DEFAULT_ACL_STRING ));
    kDebug() << " m_bLink" << (d->m_bLink == item.d->m_bLink);
    kDebug() << " size" << (size() == item.size());
    kDebug() << " ModificationTime" << (d->time(KFileItem::ModificationTime) == item.d->time(KFileItem::ModificationTime));
    kDebug() << " UDS_ICON_NAME" << (d->m_entry.stringValue( KIO::UDSEntry::UDS_ICON_NAME ) == item.d->m_entry.stringValue( KIO::UDSEntry::UDS_ICON_NAME ));
#endif
    return (
        d->m_strName == item.d->m_strName
        && d->m_bIsLocalUrl == item.d->m_bIsLocalUrl
        && d->m_fileMode == item.d->m_fileMode
        && d->m_permissions == item.d->m_permissions
        && d->user() == item.d->user()
        && d->group() == item.d->group()
        && d->m_entry.stringValue(KIO::UDSEntry::UDS_EXTENDED_ACL) == item.d->m_entry.stringValue(KIO::UDSEntry::UDS_EXTENDED_ACL)
        && d->m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING) == item.d->m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING)
        && d->m_entry.stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING) == item.d->m_entry.stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING)
        && d->m_bLink == item.d->m_bLink
        && size() == item.size()
        && d->time(KFileItem::ModificationTime) == item.d->time(KFileItem::ModificationTime) // TODO only if already known!
        && d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME) == item.d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME)
    );

    // Don't compare the mimetypes here. They might not be known, and we don't want to
    // do the slow operation of determining them here.
}

bool KFileItem::operator==(const KFileItem &other) const
{
    if (!d && !other.d) {
        return true;
    }
    if (!d || !other.d) {
        return false;
    }
    return d->m_url == other.d->m_url;
}

bool KFileItem::operator!=(const KFileItem &other) const
{
    return !operator==(other);
}

KFileItem::operator QVariant() const
{
    return qVariantFromValue(*this);
}

QString KFileItem::permissionsString() const
{
    if (!d) {
        return QString();
    }

    char buffer[12];
    char uxbit, gxbit, oxbit;

    if ((d->m_permissions & (S_IXUSR|S_ISUID)) == (S_IXUSR|S_ISUID)) {
        uxbit = 's';
    } else if ( (d->m_permissions & (S_IXUSR|S_ISUID)) == S_ISUID) {
        uxbit = 'S';
    } else if ((d->m_permissions & (S_IXUSR|S_ISUID)) == S_IXUSR) {
        uxbit = 'x';
    } else {
        uxbit = '-';
    }

    if ((d->m_permissions & (S_IXGRP|S_ISGID)) == (S_IXGRP|S_ISGID)) {
        gxbit = 's';
    } else if ( (d->m_permissions & (S_IXGRP|S_ISGID)) == S_ISGID) {
        gxbit = 'S';
    } else if ((d->m_permissions & (S_IXGRP|S_ISGID)) == S_IXGRP) {
        gxbit = 'x';
    } else {
        gxbit = '-';
    }

    if ((d->m_permissions & (S_IXOTH|S_ISVTX)) == (S_IXOTH|S_ISVTX)) {
        oxbit = 't';
    } else if ((d->m_permissions & (S_IXOTH|S_ISVTX)) == S_ISVTX) {
        oxbit = 'T';
    } else if ((d->m_permissions & (S_IXOTH|S_ISVTX)) == S_IXOTH) {
        oxbit = 'x';
    } else {
        oxbit = '-';
    }

    // Include the type in the first char like kde3 did; people are more used to seeing it,
    // even though it's not really part of the permissions per se.
    if (d->m_bLink) {
        buffer[0] = 'l';
    } else if (d->m_fileMode != KFileItem::Unknown) {
        if (S_ISDIR(d->m_fileMode)) {
            buffer[0] = 'd';
        } else if (S_ISSOCK(d->m_fileMode)) {
            buffer[0] = 's';
        } else if (S_ISCHR(d->m_fileMode)) {
            buffer[0] = 'c';
        } else if (S_ISBLK(d->m_fileMode)) {
            buffer[0] = 'b';
        } else if (S_ISFIFO(d->m_fileMode)) {
            buffer[0] = 'p';
        } else {
            buffer[0] = '-';
        }
    } else {
        buffer[0] = '-';
    }

    buffer[1] = (((d->m_permissions & S_IRUSR) == S_IRUSR) ? 'r' : '-');
    buffer[2] = (((d->m_permissions & S_IWUSR) == S_IWUSR) ? 'w' : '-');
    buffer[3] = uxbit;
    buffer[4] = (((d->m_permissions & S_IRGRP) == S_IRGRP) ? 'r' : '-');
    buffer[5] = (((d->m_permissions & S_IWGRP) == S_IWGRP) ? 'w' : '-');
    buffer[6] = gxbit;
    buffer[7] = (((d->m_permissions & S_IROTH) == S_IROTH) ? 'r' : '-');
    buffer[8] = (((d->m_permissions & S_IWOTH) == S_IWOTH) ? 'w' : '-');
    buffer[9] = oxbit;
    // if (hasExtendedACL())
    if (d->m_entry.contains(KIO::UDSEntry::UDS_EXTENDED_ACL)) {
        buffer[10] = '+';
        buffer[11] = 0;
    } else {
        buffer[10] = 0;
    }

    return QString::fromLatin1(buffer);
}

QString KFileItem::timeString(FileTimes which) const
{
    if (!d) {
        return QString();
    }
    return KGlobal::locale()->formatDateTime(d->time(which));
}

QDataStream& operator<<(QDataStream &s, const KFileItem &a)
{
    if (a.d) {
        // We don't need to save/restore anything that refresh() invalidates,
        // since that means we can re-determine those by ourselves.
        s << a.d->m_url;
        s << a.d->m_strName;
        s << a.d->m_strText;
    } else {
        s << KUrl();
        s << QString();
        s << QString();
    }

    return s;
}

QDataStream& operator>>(QDataStream &s, KFileItem &a)
{
    KUrl url;
    QString strName, strText;

    s >> url;
    s >> strName;
    s >> strText;

    if (!a.d) {
        kWarning() << "null item";
        return s;
    }

    if (url.isEmpty()) {
        a.d = nullptr;
        return s;
    }

    a.d->m_url = url;
    a.d->m_strName = strName;
    a.d->m_strText = strText;
    a.d->m_bIsLocalUrl = a.d->m_url.isLocalFile();
    a.d->m_pMimeType = nullptr;
    a.refresh();

    return s;
}

KUrl KFileItem::url() const
{
    if (!d) {
        return KUrl();
    }
    return d->m_url;
}

mode_t KFileItem::permissions() const
{
    if (!d) {
        return 0;
    }
    return d->m_permissions;
}

mode_t KFileItem::mode() const
{
    if (!d) {
        return 0;
    }
    return d->m_fileMode;
}

bool KFileItem::isLink() const
{
    if (!d) {
        return false;
    }
    return d->m_bLink;
}

bool KFileItem::isLocalFile() const
{
    if (!d) {
        return false;
    }
    return d->m_bIsLocalUrl;
}

QString KFileItem::text() const
{
    if (!d) {
        return QString();
    }
    return d->m_strText;
}

QString KFileItem::name(bool lowerCase) const
{
    if (!d) {
        return QString();
    }
    if (!lowerCase) {
        return d->m_strName;
    }
    if (d->m_strLowerCaseName.isNull()) {
        d->m_strLowerCaseName = d->m_strName.toLower();
    }
    return d->m_strLowerCaseName;
}

KUrl KFileItem::targetUrl() const
{
    if (!d) {
        return KUrl();
    }
    const QString targetUrlStr = d->m_entry.stringValue(KIO::UDSEntry::UDS_TARGET_URL);
    if (!targetUrlStr.isEmpty()) {
        return KUrl(targetUrlStr);
    }
    return url();
}

KMimeType::Ptr KFileItem::mimeTypePtr() const
{
    if (!d) {
        return KMimeType::Ptr();
    }
    if (!d->m_pMimeType) {
        // On-demand fast (but not always accurate) mimetype determination
        Q_ASSERT(!d->m_url.isEmpty());
        d->m_pMimeType = KMimeType::findByUrl(
            d->m_url, d->m_fileMode,
            // use fast mode if delayed mimetype determination can refine it later
            !d->m_bIsLocalUrl
        );
    }
    return d->m_pMimeType;
}

KIO::UDSEntry KFileItem::entry() const
{
    if (!d) {
        return KIO::UDSEntry();
    }
    return d->m_entry;
}

bool KFileItem::isMarked() const
{
    if (!d) {
        return false;
    }
    return d->m_bMarked;
}

void KFileItem::mark()
{
    if (!d) {
        kWarning() << "null item";
        return;
    }
    d->m_bMarked = true;
}

void KFileItem::unmark()
{
    if (!d) {
        kWarning() << "null item";
        return;
    }
    d->m_bMarked = false;
}

KFileItem& KFileItem::operator=(const KFileItem &other)
{
    d = other.d;
    return *this;
}

bool KFileItem::isNull() const
{
    return d == nullptr;
}

KFileItemList::KFileItemList()
{
}

KFileItemList::KFileItemList(const QList<KFileItem> &items)
    : QList<KFileItem>(items)
{
}

KFileItem KFileItemList::findByName(const QString &fileName) const
{
    const_iterator it = begin();
    const const_iterator itend = end();
    for ( ; it != itend ; ++it) {
        if ((*it).name() == fileName) {
            return *it;
        }
    }
    return KFileItem();
}

KFileItem KFileItemList::findByUrl(const KUrl &url) const
{
    const_iterator it = begin();
    const const_iterator itend = end();
    for ( ; it != itend ; ++it) {
        if ((*it).url() == url) {
            return *it;
        }
    }
    return KFileItem();
}

KUrl::List KFileItemList::urlList() const
{
    KUrl::List lst;
    lst.reserve(size());
    const_iterator it = begin();
    const const_iterator itend = end();
    for ( ; it != itend; ++it) {
        lst.append((*it).url());
    }
    return lst;
}

KUrl::List KFileItemList::targetUrlList() const
{
    KUrl::List lst;
    lst.reserve(size());
    const_iterator it = begin();
    const const_iterator itend = end();
    for ( ; it != itend ; ++it) {
        lst.append((*it).targetUrl());
    }
    return lst;
}

bool KFileItem::isDesktopFile() const
{
    // return true if desktop file
    KMimeType::Ptr mime = mimeTypePtr();
    if (!mime) {
        return false;
    }
    return mime->is("application/x-desktop");
}

bool KFileItem::isRegularFile() const
{
    if (!d) {
        return false;
    }
    return S_ISREG(d->m_fileMode);
}

QDebug operator<<(QDebug stream, const KFileItem& item)
{
    if (item.isNull()) {
        stream << "[null KFileItem]";
    } else {
        stream << "[KFileItem for" << item.url() << "]";
    }
    return stream;
}
