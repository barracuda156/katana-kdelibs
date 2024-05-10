/*
    This file is part of the KDE libraries
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#ifndef KRUN_H
#define KRUN_H

#include <kio/kio_export.h>

#include <QString>
#include <kurl.h>
#include <kservice.h>

class KRunPrivate;

/**
 * To open files with their associated applications in KDE, use KRun.
 *
 * It can execute any desktop entry, as well as any file, using
 * the default application or another application "bound" to the file type
 * (or URL protocol).
 *
 * In that example, the mimetype of the file is not known by the application,
 * so a KRun instance must be created. It will determine the mimetype by itself.
 * If the mimetype is known, or if you even know the service (application) to
 * use for this file, use one of the static methods.
 *
 * By default KRun uses auto deletion. It causes the KRun instance to delete
 * itself when the it finished its task. If you allocate the KRun
 * object on the stack you must disable auto deletion, otherwise it will crash.
 *
 * @short Opens files with their associated applications in KDE
 */
class KIO_EXPORT KRun
{
public:
    /**
     * Display the Open-With dialog for those URLs, and run the chosen application.
     * @param urls the list of applications to run
     * @param window The top-level widget of the app that invoked this object.
     * @param tempFiles if true and lst are local files, they will be deleted
     *        when the application exits.
     * @return false if the dialog was canceled
     */
    static bool displayOpenWithDialog(const KUrl::List &urls, QWidget* window,
                                      bool tempFiles = false);

    /**
     * Processes a Exec= line as found in .desktop files.
     * @param service the service to extract information from.
     * @param urls The urls the service should open.
     *
     * @return a list of arguments suitable for KProcess::setProgram().
     */
    static QStringList processDesktopExec(const KService &service, const KUrl::List &urls);

    /**
     * Given a full command line (e.g. the Exec= line from a .desktop file),
     * extract the name of the binary being run.
     * @param execLine the full command line
     * @param removePath if true, remove a (relative or absolute) path. E.g. /usr/bin/ls becomes ls.
     * @return the name of the binary to run
     */
    static QString binaryName(const QString &execLine, bool removePath);

    /**
     * Returns whether @p serviceType refers to an executable program instead
     * of a data file.
     */
    static bool isExecutable(const QString &serviceType);

    /**
     * Returns whether the @p url of @p mimetype is executable.
     * To be executable the file must pass the following rules:
     * -# Must reside on the local filesystem.
     * -# Must be marked as executable for the user by the filesystem.
     * -# The mime type must inherit application/x-executable or application/x-executable-script.
     * To allow a script to run when the above rules are satisfied add the entry
     * @code
     * X-KDE-IsAlso=application/x-executable-script
     * @endcode
     * to the mimetype's desktop file.
     */
    static bool isExecutableFile(const KUrl &url, const QString &mimetype);

    /**
     * Returns true if startup notification should be done for the given service, false otherwise
     */
    static bool checkStartupNotify(const KService *service, QByteArray *wmclass_arg);
};

#endif
