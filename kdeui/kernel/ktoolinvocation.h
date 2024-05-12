/* This file is part of the KDE libraries
    Copyright (c) 1997-1999 Matthias Kalle Dalheimer <kalle@kde.org>
    Copyright (c) 1997-2000 Matthias Ettrich <ettrich@troll.no>
    Copyright (c) 1998-2005 Stephan Kulow <coolo@kde.org>
    Copyright (c) 1999-2004 Waldo Bastian <bastian@kde.org>
    Copyright (c) 2001-2005 Lubos Lunak <l.lunak@kde.org>
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

#ifndef KTOOLINVOCATION_H
#define KTOOLINVOCATION_H

#include <kdecore_export.h>

#include <QStringList>
#include <QDBusInterface>

class KUrl;

/**
 * KToolInvocation starts other programs
 */
class KDECORE_EXPORT KToolInvocation : public QObject
{
    Q_OBJECT
private:
public:
    KToolInvocation(QObject *parent = nullptr);
    ~KToolInvocation();

    static KToolInvocation *self();

public Q_SLOTS:
   /**
     * Invokes the help viewer.
     *
     * @param anchor      This has to be a defined anchor in your docbook sources. If empty the
     *                    main index is loaded
     * @param appname     This allows you to show the help of another application. If empty the
     *                    current name() is used
     */
    void invokeHelp(const QString &anchor = QString(), const QString &appname = QString());

    /**
     * Convenience method; invokes the standard email application.
     *
     * @param address     The destination address
     */
    void invokeMailer(const QString &address);

    /**
     * Invokes the user's preferred browser. Note that you should only do this when you know for
     * sure that the browser can handle the URL (i.e. its mimetype). In doubt, if the URL can point
     * to an image or anything else than HTML, prefer to use KRun.
     *
     * @param url         The destination address
     */
    void invokeBrowser(const QString &url);

    /**
     * Invokes the standard terminal application.
     *
     * @param command     The command to execute, can be empty.
     * @param workdir     The initial working directory, can be empty.
     *
     * @since 4.1
     */
    void invokeTerminal(const QString &command, const QString &workdir = QString());

public:
    /**
     * Set environment variable of the launcher, e.g. "SESSION_MANAGER"
     *
     * @param name   The environment variable name
     * @param value  The environment variable value
     */
    void setLaunchEnv(const QString &name, const QString &value);

    /**
     * Starts a service based on the MIME type of the URL, the MIME type is automatically
     * determined
     *
     * @param url     The URL to start service for
     * @param window  Window to use for error reporting and job delegation
     * @param temp    Whether the URL is temporary file or not
     * @return an error code indicating success (== 0) or failure (> 0).
     */
    bool startServiceForUrl(const QString &url, QWidget *window = nullptr, bool temp = false);

    /**
     * Starts a service based on the desktop name or entry path of the service, e.g. "konqueror"
     *
     * @param name    The desktop name of the service
     * @param urls    If not empty these URLs will be passed to the service
     * @param window  Window to use for error reporting and job delegation
     * @param temp    Whether any of the URLs is temporary file or not
     * @return an error code indicating success (== 0) or failure (> 0)
     */
    bool startServiceByStorageId(const QString &name, const QStringList &urls = QStringList(),
                                 QWidget *window = nullptr, bool temp = false);

    /**
     * Starts a program.
     *
     * @param name    Name of the program to start
     * @param args    Arguments to pass to the program
     * @param window  Window to use for error reporting and job delegation
     * @param temp    Whether any of the arguments is temporary file or not
     * @return an error code indicating success (== 0) or failure (> 0)
     */
    bool startProgram(const QString &name, const QStringList &args = QStringList(),
                      QWidget *window = nullptr, bool temp = false);

private:
    Q_DISABLE_COPY(KToolInvocation);

    /**
     * @internal
     */
    bool startServiceInternal(const char *_function,
                              const QString &name, const QStringList &urls,
                              QWidget *window, const bool temp,
                              const QString &workdir = QString());

    QDBusInterface *klauncherIface;
};

#endif

