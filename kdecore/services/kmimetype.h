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

#ifndef KMIMETYPE_H
#define KMIMETYPE_H

#include <kurl.h>
#include <ksharedptr.h>

class KMimeTypePrivate;

/**
 * Represent a mime type like "text/plain" and the data that is associated with it.
 *
 * The starting point you need is often the static methods.
 */
class KDECORE_EXPORT KMimeType : public QSharedData
{
    Q_DECLARE_PRIVATE(KMimeType)
public:
    typedef KSharedPtr<KMimeType> Ptr;
    typedef QList<Ptr> List;

    ~KMimeType();

    /**
     * Return the name of the mimetype, e.g. "text/plain".
     *
     * @return the name of the mimetype.
     */
    QString name() const;

    /**
     * Return the filename of the icon associated with the mimetype. Use
     * KIconLoader::loadMimeTypeIcon to load the icon.
     *
     * @param url argument only used for directories, where the icon can be specified in the
     *        .directory file.
     *
     * @return the name of the icon associated with this MIME type.
     */
    QString iconName(const KUrl &url = KUrl()) const;

    /**
     * Return the filename of the icon associated with the mimetype for the given @p url. Use
     * KIconLoader::loadMimeTypeIcon to load the icon.
     *
     * @param url URL for the file
     * @param mode the mode of the file. The mode may modify the icon with overlays that show
     *             special properties of the icon. Use 0 for default
     * @return the name of the icon. The name of a default icon if there is no icon for the mime
     *         type
     */
    static QString iconNameForUrl(const KUrl &url, mode_t mode = 0);

    /**
     * Return the "favicon" for the given @p url if available. Does NOT attempt to download the
     * favicon by default, it only returns one that is already available. If @p download is true
     * then an attempt will be made to download icon for the given @p url but it will not be
     * available immediately.
     *
     * @param url the URL of the favicon
     * @return the name of the favicon, if unavailable returns QString().
     * @see http://www.favicon.com
     */
    static QString favIconForUrl(const KUrl &url, bool download = false);

    /**
     * Returns the descriptive comment associated with the MIME type. The @p url argument is used
     * for folders.
     *
     * @return The descriptive comment associated with the MIME type, if any.
     */
    QString comment(const KUrl &url = KUrl()) const;

    /**
     * Retrieve the list of patterns associated with the MIME Type.
     *
     * @return a list of file globs that describe the file names (or, usually, the extensions) of
     *         files with this mime type
     */
    QStringList patterns() const;

    enum FindByNameOption {
        DontResolveAlias = 0,
        ResolveAliases = 1
    };

    /**
     * Retrieve a pointer to the mime type @p name
     *
     * @em Very @em important: Don't store the result in a KMimeType* !
     *
     * Also note that a new KMimeType pointer is created every time by this call. Don't ever write
     * code that compares mimetype pointers, compare names instead.
     *
     * @param name the name of the mime type
     * @param options controls how the mime type is searched for
     * @return the pointer to the KMimeType with the given @p name or null if not found
     * @see KServiceType::serviceType
     */
    static Ptr mimeType(const QString &name, FindByNameOption options = ResolveAliases);

    /**
     * Finds a KMimeType for the given @p url. This function looks at mode_t first, if that does
     * not help it looks at the name. If no extension matches, then the file contents will be
     * examined if the URL is a local file. If nothing matches default MIME type is returned.
     *
     * @param path the path to the file
     * @param mode the mode of the file (used to identify sockets, FIFOs, etc.)
     * @param fast_mode If set to true the file contents is not examined. The result may be
     *                  suboptimal but it is @em fast.
     * @param accuracy If not a null pointer accuracy is set to the accuracy of the match (which is
     *                 in the range 0..100)
     * @return A pointer to the matching mimetype. null is never returned.
     */
    static Ptr findByUrl(const KUrl &url, mode_t mode = 0,
                          bool fast_mode = false, int* accuracy = 0);

    /**
     * Tries to find out the MIME type of a file by matching its name to MIME type rules. Note that
     * the file's contents is not used.
     *
     * @param fileName the path to the file
     * @param accuracy If not a null pointer, *accuracy is set to the accuracy of the match (which
     *                 is in the range 0..100)
     * @return a pointer to the KMimeType or the default mimetype if the type cannot be determined.
     */
    static Ptr findByName(const QString &fileName, int *accuracy = 0);

    /**
     * Tries to find out the MIME type of a data chunk by looking for  certain magic numbers and
     * characteristics in it.
     *
     * @param data the data to examine
     * @param accuracy If not a null pointer, *accuracy is set to the accuracy of the match (which
     *                 is in the range 0..100)
     * @return a pointer to the KMimeType or default mimetype if the type cannot be determined.
     */
    static Ptr findByContent(const QByteArray &data, int *accuracy = 0);

    /**
     * Returns whether a file has an internal format that is not human readable. This is much more
     * generic than "not mime->is(text/plain)". Many application file formats (like rtf and
     * postscript) are based on text but are not something that the user should rarely ever see.
     */
    static bool isBinaryData(const QString &fileName);

    /**
     * Same as isBinaryData() except it uses chunk of data.
     */
    static bool isBufferBinaryData(const QByteArray &data);

    /**
     * Get all the mimetypes. Useful for showing the list of available mimetypes. Don't use unless
     * really necessary.
     *
     * @return the list of all existing KMimeTypes
     */
    static List allMimeTypes();

    /**
     * Returns the name of the default mimetype
     *
     * @return the name of the default mime type, always "application/octet-stream"
     */
    static QString defaultMimeType();

    /**
     * Returns the default mimetype.
     *
     * @return the "application/octet-stream" mimetype pointer.
     */
    static KMimeType::Ptr defaultMimeTypePtr();

    /**
     * @return Return true if this mimetype is the default mimetype, false otherwise.
     */
    bool isDefault() const;

    /**
     * If this mimetype is a subclass of one or more other mimetypes return the list of those
     * mimetypes.
     *
     * For instance a application/javascript is a special kind of text/plain, so the definition of
     * "application/javascript" says sub-class-of type="text/plain"
     *
     * Another example: "application/x-shellscript" is a subclass of two other mimetypes,
     * "application/x-executable" and "text/plain".
     *
     * @note this notion doesn't map to the servicetype inheritance mechanism since an application
     * that handles the specific type doesn't necessarily handle the base type. The opposite is
     * true though.
     *
     * @return the list of parent mimetypes
     * @since 4.1
     */
    QStringList parentMimeTypes() const;

    /**
     * Return all parent mimetypes of this mimetype, direct or indirect. This includes the
     * parent(s) of its parent(s), etc. If this mimetype is an alias, the list also contains the
     * canonical name for this mimetype.
     *
     * The usual reason to use this method is to look for a setting which is stored per mimetype
     * (like PreviewJob does).
     *
     * @since 4.1
     */
    QStringList allParentMimeTypes() const;

    /**
     * Do not use name() == "somename" anymore, to check for a given mimetype. For mimetype
     * inheritance to work, use is("somename") instead.
     *
     * @note this method also checks mimetype aliases.
     */
    bool is(const QString &mimeTypeName) const;

    /**
     * Returns the user-specified icon for this mimetype. This is empty most of the time, you
     * probably want to use iconName() instead. This method is for the mimetype editor.
     *
     * @since 4.3
     */
    QString userSpecifiedIconName() const;

    /**
     * Return the primary extension associated with this mimetype, if any. If patterns() returns
     * (*.jpg, *.jpeg) then mainExtension will return ".jpg". Note that the dot is included.
     *
     * If none of the patterns is in *.foo format (for instance <code>*.jp? or *.* or
     * callgrind.out* </code>) then mainExtension() returns an empty string.
     *
     * @since 4.3
     */
    QString mainExtension() const;

    /**
     * Determines the extension from a filename (or full path) using the mimetype database. This
     * allows to extract "tar.bz2" for foo.tar.bz2 but still return "txt" for my.doc.with.dots.txt
     */
    static QString extractKnownExtension(const QString &fileName);

    /**
     * Returns true if the given filename matches the given pattern.
     *
     * @since 4.6.1
     */
    static bool matchFileName(const QString &filename, const QString &pattern);

    /**
     * Returns the version of the installed update-mime-database program (from
     * freedesktop.org shared-mime-info). This is used by unit tests  and by the code that writes
     * out icon definitions.
     *
     * @since 4.3
     * @return -1 on error, otherwise a version number to use like this:
     * @code
     * if (version >= KDE_MAKE_VERSION(0, 40, 0)) { ... }
     * @endcode
     */
    static int sharedMimeInfoVersion();

protected:
    friend class KMimeTypeRepository; // for KMimeType(QString,QString)

    /**
     * Construct a mimetype and take all information from an XML file.
     *
     * @param fullpath the path to the xml that describes the mime type
     * @param name the name of the mimetype (usually the end of the path)
     */
    KMimeType(const QString &fullpath, const QString &name);

private:
    KMimeTypePrivate* d_ptr;
};

#endif // KMIMETYPE_H
