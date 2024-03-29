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
#ifndef KPROTOCOLMANAGER_H
#define KPROTOCOLMANAGER_H

#include <QtCore/QStringList>

#include <kio/global.h>
#include "kprotocolinfo.h"

class KSharedConfig;
template<class T>
class KSharedPtr;
typedef KSharedPtr<KSharedConfig> KSharedConfigPtr;

namespace KIO
{
    class SlaveConfigPrivate;
} // namespace KIO

/**
 * Provides information about I/O (Internet, etc.) settings chosen/set by the end user.
 *
 * KProtocolManager has a heap of static functions that allows only read access to KDE's IO related
 * settings. These include proxy, file transfer resumption, timeout and user-agent related
 * settings.
 *
 * The information provided by this class is generic enough to be applicable to any application
 * that makes use of KDE's IO sub-system.  Note that this mean the proxy, timeout etc. settings
 * are saved in a separate user-specific config file and not in the config file of the application.
 *
 * Original author:
 * @author Torben Weis <weis@kde.org>
 *
 * Revised by:
 * @author Waldo Bastain <bastain@kde.org>
 * @author Dawit Alemayehu <adawit@kde.org>
 */
class KIO_EXPORT KProtocolManager
{
public:
    /*=========================== USER-AGENT SETTINGS ===========================*/
    /**
     * Returns the default user-agent string used for web browsing.
     */
    static QString defaultUserAgent();

    /*=========================== TIMEOUT CONFIG ================================*/
    /**
     * Returns the preferred timeout value for reading from remote connections in seconds.
     */
    static int readTimeout();

    /**
     * Returns the preferred timeout value for remote connections in seconds.
     */
    static int connectTimeout();

    /**
     * Returns the preferred response timeout value for remote connecting in seconds.
     */
    static int responseTimeout();

    /*============================ DOWNLOAD CONFIG ==============================*/
    /**
     * Returns true if partial downloads should be automatically resumed.
     */
    static bool autoResume();

    /**
     * Returns true if partial downloads should be marked with a ".part" extension.
     */
    static bool markPartial();

    /**
     * Returns the minimum file size for keeping aborted downloads.
     *
     * Any data downloaded that does not meet this minimum requirement will simply be discarded.
     * The default size is 5 KB.
     */
    static int minimumKeepSize();

    /*===================== PROTOCOL CAPABILITIES ===============================*/
    /**
     * Returns whether the protocol can list files/objects. If a protocol supports listing it can
     * be browsed in e.g. file managers.
     *
     * Whether a protocol supports listing is determined by the "listing=" field in the protocol
     * description file. If the protocol support listing it should list the fields it provides in
     * this field. If the protocol does not support listing this field should remain empty
     * (default).
     *
     * @param url the url to check
     */
    static bool supportsListing(const KUrl &url);

    /**
     * Returns whether the protocol can retrieve data from URLs.
     *
     * This corresponds to the "reading=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsReading(const KUrl &url);

    /**
     * Returns whether the protocol can store data to URLs.
     *
     * This corresponds to the "writing=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsWriting(const KUrl &url);

    /**
     * Returns whether the protocol can create directories/folders.
     *
     * This corresponds to the "makedir=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsMakeDir(const KUrl &url);

    /**
     * Returns whether the protocol can delete files/objects.
     *
     * This corresponds to the "deleting=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsDeleting(const KUrl &url);

    /**
     * Returns whether the protocol can create links between files/objects.
     *
     * This corresponds to the "linking=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsLinking(const KUrl &url);

    /**
     * Returns whether the protocol can move files/objects between different locations.
     *
     * This corresponds to the "moving=" field in the protocol description file. Valid values for
     * this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool supportsMoving(const KUrl &url);

    /**
     * Returns whether the protocol can copy files/objects directly from the filesystem itself. If
     * not, the application will read files from the filesystem using the file-protocol and pass
     * the data on to the destination protocol.
     *
     * This corresponds to the "copyFromFile=" field in the protocol description file. Valid values
     * for this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool canCopyFromFile(const KUrl &url);

    /**
     * Returns whether the protocol can copy files/objects directly to the filesystem itself. If
     * not, the application will receive the data from the source protocol and store it in the
     * filesystem using the file-protocol.
     *
     * This corresponds to the "copyToFile=" field in the protocol description file. Valid values
     * for this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool canCopyToFile(const KUrl &url);

    /**
     * Returns whether the protocol can rename (i.e. move fast) files/objects directly from the
     * filesystem itself. If not, the application will read files from the filesystem using the
     * file-protocol and pass the data on to the destination protocol.
     *
     * This corresponds to the "renameFromFile=" field in the protocol description file. Valid
     * values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool canRenameFromFile(const KUrl &url);

    /**
     * Returns whether the protocol can rename (i.e. move fast) files/objects directly to the
     * filesystem itself. If not, the application will receive the data from the source protocol
     * and store it in the filesystem using the file-protocol.
     *
     * This corresponds to the "renameToFile=" field in the protocol description file. Valid values
     * for this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool canRenameToFile(const KUrl &url);

    /**
     * Returns whether the protocol can recursively delete directories by itself. If not (the usual
     * case) then KIO will list the directory and delete files and empty directories one by one.
     *
     * This corresponds to the "deleteRecursive=" field in the protocol description file. Valid
     * values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     */
    static bool canDeleteRecursive(const KUrl &url);

    /**
     * This setting defines the strategy to use for generating a filename, when copying a file or
     * directory to another directory. By default the destination filename is made out of the
     * filename in the source URL. However if the ioslave displays names that are different from
     * the filename of the URL (e.g. kio_trash shows foo.txt and uses some internal URL), using
     * Name means that the display name (UDS_NAME) will be used to as the filename in the
     * destination directory.
     *
     * This corresponds to the "fileNameUsedForCopying=" field in the protocol description file.
     * Valid values for this field are "Name" or "FromURL" (default).
     *
     * @param url the url to check
     */
    static KProtocolInfo::FileNameUsedForCopying fileNameUsedForCopying(const KUrl &url);

    /**
     * Returns default mimetype for this URL based on the protocol, empty string if unknown.
     *
     * This corresponds to the "defaultMimetype=" field in the protocol description file.
     *
     * @param url the url to check
     */
    static QString defaultMimetype(const KUrl &url);

    /**
     * Returns whether the protocol can act as a source protocol.
     *
     * A source protocol retrieves data from or stores data to the location specified by a URL.
     * A source protocol (e.g. http) is the opposite of a filter protocol (e.g. filenamesearch).
     *
     * The "source=" field in the protocol description file determines whether a protocol is a
     * source protocol or a filter protocol.
     * @param url the url to check
     */
    static bool isSourceProtocol(const KUrl &url);

    /*=============================== OTHERS ====================================*/
    /**
     * Force a reload of the general config file of io-slaves (kioslaverc).
     */
    static void reparseConfiguration();

    /**
     * Return Accept-Languages header built up according to user's desktop language settings.
     */
    static QString acceptLanguagesHeader();

    /**
     * Returns the charset to use for the specified @ref url.
     *
     * @since 4.10
     */
    static QString charsetFor(const KUrl &url);

private:
    friend class KIO::SlaveConfigPrivate;

    /**
     * @internal
     * (Shared with SlaveConfig)
     */
    KIO_NO_EXPORT static KSharedConfigPtr config();
};

#endif // KPROTOCOLMANAGER_H
