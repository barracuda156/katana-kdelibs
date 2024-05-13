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
 * KRun provides utility methods for running services and applications
 */
class KIO_EXPORT KRun
{
public:
    /**
     * Display the Open-With dialog for URLs and run the chosen application
     *
     * @param urls the list of URLs to run
     * @param window the top-level widget of the application, if any
     * @param temp if true any local file URL will be deleted when the application exits
     * @return false if the dialog was canceled
     */
    static bool displayOpenWithDialog(const KUrl::List &urls, QWidget *window, bool temp = false);

    /**
     * Processes a Exec= line as found in .desktop files
     *
     * @param service the service to extract information from
     * @param urls the urls the service should open
     * @return a list of arguments suitable for KProcess::setProgram()
     * @warning caller is responsible for handling services which do not support remote URLs
     */
    static QStringList processDesktopExec(const KService &service, const QStringList &urls);

    /**
     * Given a full command line (e.g. the Exec= line from a .desktop file), extracts the name of
     * the binary being run.
     *
     * @param execLine the full command line
     * @param removePath if true, remove a (relative or absolute) path. e.g. /usr/bin/ls becomes ls
     * @return the name of the binary to run
     */
    static QString binaryName(const QString &execLine, bool removePath);

    /**
     * Returns whether @p mimeType refers to an executable program
     */
    static bool isExecutable(const QString &mimeType);

    /**
     * Returns whether the @p url of @p mimetype is executable
     */
    static bool isExecutableFile(const KUrl &url, const QString &mimetype);

    /**
     * Returns true if startup notification should be done for the given service, false otherwise
     */
    static bool checkStartupNotify(const KService *service, QByteArray *wmclass_arg);
};

#endif
