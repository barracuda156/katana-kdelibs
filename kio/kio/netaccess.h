/*
    This file is part of the KDE libraries
    Copyright (C) 1997 Torben Weis (weis@kde.org)
    Copyright (C) 1998 Matthias Ettrich (ettrich@kde.org)
    Copyright (C) 1999-2004 David Faure (faure@kde.org)

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

#ifndef KIO_NETACCESS_H
#define KIO_NETACCESS_H

#include <QObject>
#include <QStringList>
#include <QWidget>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <kurl.h>
#include <kio/jobclasses.h>

class KJob;
namespace KIO {

class Job;
class NetAccessPrivate;

/**
 * Net Transparency.
 *
 * NetAccess allows you to do simple file operation (load, save, copy, delete...) without working
 * with KIO::Job directly. Whereas a KIO::Job is asynchronous, meaning that the developer has to
 * connect slots for it, KIO::NetAccess provides synchronous downloads and uploads, as well as
 * temporary file creation and removal. The functions appear to be blocking, but the event loop
 * continues running while the operations are handled. If possible, use async KIO jobs instead!
 * See the documentation of KJob::exec() for more about the dangers of NetAccess.
 *
 * This class isn't meant to be used as a class but only as a simple namespace for static
 * functions, though an instance of the class is built for internal purposes.
 *
 * Port to kio done by David Faure, faure@kde.org
 *
 * @short Provides a blocking interface to KIO file operations.
 */
class KIO_EXPORT NetAccess : public QObject
{
    Q_OBJECT

public:
    enum StatSide {
        SourceSide,
        DestinationSide
    };

    /**
     * Downloads a file from an arbitrary URL (@p src) to a temporary file on the local filesystem
     * (@p target).
     *
     * If the argument for @p target is an empty string, download will generate a unique temporary
     * filename in /tmp. Since @p target is a reference to QString you can access this filename
     * easily. Download will return true if the download was successful, otherwise false.
     *
     * Special case:
     * If the URL is of kind file:, then no downloading is processed but the full filename is
     * returned in @p target. That means you @em have to take care about the @p target argument.
     * (This is very easy to do, please see the example below.)
     *
     * Download is synchronous. That means it can be used like this (assuming the application has
     * a loadFile() function):
     *
     * \code
     * QString tmpFile;
     * if( KIO::NetAccess::download(url, tmpFile, window)) {
     *     loadFile(tmpFile);
     *     KIO::NetAccess::removeTempFile(tmpFile);
     * } else {
     *     KMessageBox::error(this, KIO::NetAccess::lastErrorString());
     * }
     * \endcode
     *
     * Of course, user interface will still process events during the download. If the download
     * fails lastError() and lastErrorString() will be set.
     *
     * If the url is always remote, then it can be used the more usual way:
     * \code
     * KTemporaryFile tmpFile;
     * if (tmpFile.open()) {
     *     KIO::Job* getJob = KIO::file_copy(url, KUrl(tmpFile.fileName()), -1, KIO::Overwrite | KIO::HideProgressInfo);
     *     getJob->ui()->setWindow(window);
     *     if (KIO::NetAccess::synchronousRun(getJob, 0)) {
     *         loadFile(tmpFile.fileName());
     *     } else {
     *         getJob->ui()->showErrorMessage();
     *     }
     * }
     * \endcode
     *
     * @param src URL Reference to the file to download.
     * @param target String containing the final local location of the file.  If you insert an
     *               empty string, it will return a location in a temporary spot. <B>Note:</B>
     *               you are responsible for the removal of this file when you are finished reading
     *               it using removeTempFile.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if successful, false for failure.  Use lastErrorString() to get the reason it
     *         failed.
     *
     * @see lastErrorString()
     */
    static bool download(const KUrl &src, QString &target, QWidget *window);

    /**
     * Removes the specified file if and only if it was created by KIO::NetAccess as a temporary
     * file for a former download.
     *
     * @param name Path to temporary file to remove, may not be empty.
     */
    static void removeTempFile(const QString &name);

    /**
     * Uploads file @p src to URL @p target.
     *
     * Both must be specified, unlike download. Note that this is assumed to be used for saving a
     * file over the network, so overwriting is set to true. This is not the case with copy.
     *
     * @param src URL Referencing the file to upload.
     * @param target URL containing the final location of the file.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if successful, false for failure
     */
    static bool upload(const QString &src, const KUrl &target, QWidget *window);

    /**
     * Alternative to upload for copying over the network. Overwrite is false, so this will fail
     * if @p target exists.
     *
     * This one takes two URLs and is a direct equivalent of KIO::file_copy.
     *
     * @param src URL Referencing the file to upload.
     * @param target URL containing the final location of the file.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if successful, false for failure
     */
    static bool file_copy(const KUrl &src, const KUrl &target, QWidget *window = nullptr);

    /**
     * Alternative method for copying over the network.
     *
     * This one takes two URLs and is a direct equivalent of KIO::copy!. This means that it can
     * copy files and directories alike (it should have been named copy()).
     *
     * This method will bring up a dialog if the destination already exists.
     *
     * @param src URL Referencing the file to upload.
     * @param target URL containing the final location of the
     *               file.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if successful, false for failure
     */
    // TODO deprecate in favor of KIO::copy + synchronousRun (or job->exec())
    static bool dircopy(const KUrl &src, const KUrl &target, QWidget *window);

    /**
     * Overloaded method, which takes a list of source URLs
     */
    // TODO deprecate in favor of KIO::copy + synchronousRun (or job->exec())
    static bool dircopy(const KUrl::List &src, const KUrl &target, QWidget *window = nullptr);

    /**
     * Tests whether a URL exists.
     *
     * @param url the URL we are testing
     * @param statSide determines if we want to read or write.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if the URL exists and we can do the operation specified by @p source, false
     *         otherwise
     *
     * @see KIO::stat()
     */
    static bool exists(const KUrl &url, StatSide statSide, QWidget *window);

    /**
     * Tests whether a URL exists and return information on it.
     *
     * This is a convenience function for KIO::stat().
     *
     * @param url The URL we are testing.
     * @param entry The result of the stat. Iterate over the list of atoms to get hold of name,
     *              type, size, etc., or use KFileItem.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true if successful, false for failure
     */
    static bool stat(const KUrl &url, KIO::UDSEntry &entry, QWidget *window);

    /**
     * Tries to map a local URL for the given URL.
     *
     * This is a convenience function for KIO::stat() + parsing the resulting UDSEntry.
     *
     * @param url The URL we are testing.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return a local URL corresponding to the same resource than the original URL, or the
     *         original URL if no local URL can be mapped
     */
    static KUrl mostLocalUrl(const KUrl &url, QWidget *window);

    /**
     * Deletes a file or a directory in a synchronous way.
     *
     * This is a convenience function for KIO::del().
     *
     * @param url The file or directory to delete.
     * @param window main window associated with this job. This is used to show message boxes.
     *
     * @return true on success, false on failure.
     */
    static bool del(const KUrl &url, QWidget *window);

    /**
     * Creates a directory in a synchronous way.
     *
     * This is a convenience function for KIO::mkdir().
     *
     * @param url The directory to create.
     * @param window main window associated with this job. This is used to show message boxes.
     * @param permissions directory permissions.
     *
     * @return true on success, false on failure.
     */
    static bool mkdir(const KUrl &url, QWidget *window, int permissions = -1);

    /**
     * This function executes a job in a synchronous way.
     *
     * If a job fetches some data, pass a QByteArray pointer as data parameter to this function
     * and after the function returns it will contain all the data fetched by this job.
     *
     * @code
     * KIO::Job *job = KIO::get(url);
     * KIO::MetaData metaData;
     * metaData.insert("no-auth", "yes");
     * if (NetAccess::synchronousRun(job, 0, &data, &url, &metaData)) {
     *     kDebug()<<"Success";
     * }
     * @endcode
     *
     * @param job job which the function will run. Note that after this function finishes running,
     *            job is deleted and you can't access it anymore!
     * @param window main window associated with this job. This is used to show message boxes.
     * @param data if passed and relevant to this job then it will contain the data that was
     *             fetched by the job
     * @param finalURL if passed will contain the final url of this job (it might differ from the
     *                 one it was created with if there was a redirection)
     * @param metaData you can pass a pointer to the map with meta data you wish to set on the job.
     *                 After the job finishes this map will hold all the meta data from the job.
     *
     * @return true on success, false on failure.
     */
    static bool synchronousRun(Job *job, QWidget *window, QByteArray *data = nullptr,
                                KUrl* finalURL = nullptr, MetaData *metaData = nullptr);

    /**
     * Returns the error string for the last job, in case it failed. Note that the error is already
     * translated.
     *
     * @return the last error string, or QString()
     */
    static QString lastErrorString();

    /**
     * Returns the error code for the last job, in case it failed.
     *
     * @return the last error code
     */
    static int lastError();

Q_SIGNALS:
    void leaveModality();

private:
    NetAccess();
    ~NetAccess();

    bool filecopyInternal(const KUrl &src, const KUrl &target, int permissions,
                          KIO::JobFlags flags, QWidget *window, bool move);
    bool dircopyInternal(const KUrl::List &src, const KUrl &target,
                         QWidget *window, bool move);
    bool statInternal(const KUrl & url, int details, StatSide side, QWidget* window);

    bool delInternal(const KUrl &url, QWidget *window);
    bool mkdirInternal(const KUrl &url, int permissions, QWidget *window);
    bool synchronousRunInternal(Job *job, QWidget *window, QByteArray *data,
                                KUrl *finalURL, MetaData* metaData);

    void enter_loop();

private Q_SLOTS:
    void slotResult(KJob *job);
    void slotData(KIO::Job *job, const QByteArray &data);
    void slotRedirection(KIO::Job *job, const KUrl &url);
    void slotShowProgress();

private:
    NetAccessPrivate * const d;
};

}

#endif // KIO_NETACCESS_H
