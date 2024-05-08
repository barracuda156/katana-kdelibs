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
     * Invokes the KHelpCenter HTML help viewer from docbook sources.
     *
     * @param anchor      This has to be a defined anchor in your docbook sources. If empty the
     *                    main index is loaded
     * @param appname     This allows you to show the help of another application. If empty the
     *                    current name() is used
     * @param startup_id  For app startup notification, "0" for none
     */
    void invokeHelp(const QString &anchor = QString(), const QString &appname = QString(),
                    const QByteArray &startup_id = QByteArray());

    /**
     * Convenience method; invokes the standard email application.
     *
     * @param address     The destination address
     * @param subject     Subject string. Can be QString().
     * @param startup_id  Ffor app startup notification, "0" for none
     */
    void invokeMailer(const QString &address, const QString &subject,
                      const QByteArray &startup_id = QByteArray());

    /**
     * Invokes the standard email application.
     *
     * @param mailtoURL         A mailto URL.
     * @param startup_id        For app startup notification, "0" for none
     * @param allowAttachments  Whether attachments specified in mailtoURL should be honoured. The
     *                          default is false; do not honor requests for attachments.
     */
    void invokeMailer(const KUrl &mailtoURL, const QByteArray &startup_id = QByteArray(),
                      bool allowAttachments = false);

    /**
     * Convenience method; invokes the standard email application.
     *
     * All parameters are optional.
     *
     * @param to          The destination address.
     * @param cc          The Cc field
     * @param subject     Subject string
     * @param body        A string containing the body of the mail
     * @param attachURLs  List of URLs to be attached to the mail.
     * @param startup_id  For app startup notification, "0" for none
     */
    void invokeMailer(const QString &to, const QString &cc, const QString &subject,
                      const QString &body, const QStringList &attachURLs = QStringList(),
                      const QByteArray &startup_id = QByteArray());

    /**
     * Invokes the user's preferred browser. Note that you should only do this when you know for
     * sure that the browser can handle the URL (i.e. its mimetype). In doubt, if the URL can point
     * to an image or anything else than HTML, prefer to use KRun.
     *
     * @param url         The destination address
     * @param startup_id  For app startup notification, "0" for none
     */
    void invokeBrowser(const QString &url, const QByteArray &startup_id = QByteArray());

    /**
     * Invokes the standard terminal application.
     *
     * @param command     The command to execute, can be empty.
     * @param workdir     The initial working directory, can be empty.
     * @param startup_id  For app startup notification, "0" for none
     *
     * @since 4.1
     */
    void invokeTerminal(const QString &command, const QString &workdir = QString(),
                        const QByteArray &startup_id = QByteArray());

public:
    /**
     * Set environment variable of the launcher, e.g. "SESSION_MANAGER"
     *
     * @param name   The environment variable name
     * @param value  The environment variable value
     */
    void setLaunchEnv(const QString &name, const QString &value);

    /**
     * Starts a service based on the desktop path of the service, e.g.
     * "Applications/konqueror.desktop" or "/home/user/bla/myfile.desktop"
     *
     * @param name        The path of the desktop file
     * @param URL         If not empty this URL is passed to the service
     * @param error       On failure, @p error contains a description of the error that occurred.
     *                    If the pointer is null, the argument will be ignored
     * @param startup_id  For app startup notification, "0" for none
     * @return an error code indicating success (== 0) or failure (> 0).
     */
    int startServiceByDesktopPath(const QString &name, const QString &URL,
                                  QString *error = nullptr,
                                  const QByteArray &startup_id = QByteArray());

    /**
     * Starts a service based on the desktop path of the service, e..g.
     * "Applications/konqueror.desktop" or "/home/user/bla/myfile.desktop"
     *
     * @param name        The path of the desktop file
     * @param URLs        If not empty these URLs will be passed to the service
     * @param error       On failure, @p error contains a description of the error that occurred.
     *                    If the pointer is null, the argument will be ignored
     * @param startup_id  For app startup notification, "0" for none
     * @return an error code indicating success (== 0) or failure (> 0).
     */
    int startServiceByDesktopPath(const QString &name, const QStringList &URLs = QStringList(),
                                  QString *error = nullptr,
                                  const QByteArray &startup_id = QByteArray());

  /**
   * Starts a service based on the desktop name of the service, e.g. "konqueror"
   *
   * @param name        The desktop name of the service
   * @param URL         If not empty this URL is passed to the service
   * @param error       On failure, @p error contains a description of the error that occurred.
   *                    If the pointer is null, the argument will be ignored
   * @param startup_id  For app startup notification, "0" for none
   * @return an error code indicating success (== 0) or failure (> 0)
   */
    int startServiceByDesktopName(const QString &name, const QString &URL,
                                  QString *error = nullptr,
                                  const QByteArray &startup_id = QByteArray());

    /**
     * Starts a service based on the desktop name of the service, e.g. "konqueror"
     *
     * @param name        The desktop name of the service
     * @param URLs        If not empty these URLs will be passed to the service
     * @param error       On failure, @p error contains a description of the error that occurred.
     *                    If the pointer is null, the argument will be ignored
     * @param startup_id  For app startup notification, "0" for none
     * @return an error code indicating success (== 0) or failure (> 0)
     */
    int startServiceByDesktopName(const QString &name, const QStringList &URLs = QStringList(),
                                  QString *error = nullptr,
                                  const QByteArray &startup_id = QByteArray());

    /**
     * Starts a program via kdeinit.
     *
     * @param name        Name of the program to start
     * @param args        Arguments to pass to the program
     * @param error       On failure, @p error contains a description of the error that occurred
     *                    If the pointer is null, the argument will be ignored
     * @param startup_id  For app startup notification, "0" for none
     * @return an error code indicating success (== 0) or failure (> 0)
     */
    int kdeinitExec(const QString &name, const QStringList &args = QStringList(),
                    QString *error = nullptr, const QByteArray &startup_id = QByteArray());

    /**
     * Starts a program via kdeinit and wait for it to finish, it behaves similar to the system()
     * function.
     *
     * @param name        Name of the program to start
     * @param args        Arguments to pass to the program
     * @param error       On failure, @p error contains a description of the error that occurred
     *                    If the pointer is null, the argument will be ignored
     * @param startup_id  For app startup notification, "0" for none
     * @return an error code indicating success (== 0) or failure (> 0)
     */
    int kdeinitExecWait(const QString &name, const QStringList &args = QStringList(),
                        QString *error = nullptr, const QByteArray &startup_id = QByteArray());

private:
    Q_DISABLE_COPY(KToolInvocation);

    /**
     * @internal
     */
    int startServiceInternal(const char *_function,
                             const QString &name, const QStringList &URLs,
                             QString *error,
                             const QByteArray &startup_id,
                             const QString &workdir = QString());

    QDBusInterface *klauncherIface;
};

#endif

