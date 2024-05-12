/*  This file is part of the KDE libraries
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#ifndef KLAUNCHER_ADAPTOR_H
#define KLAUNCHER_ADAPTOR_H

#include "kstartupinfo.h"
#include "kservice.h"

#include <QDBusAbstractAdaptor>
#include <QProcess>
#include <QTimer>

// #define KLAUNCHER_DEBUG

class KLauncherProcess : public QProcess
{
    Q_OBJECT
public:
    explicit KLauncherProcess(QObject *parent);
    ~KLauncherProcess();

    void setupStartup(const QString &appexe, const KService::Ptr kservice, const qint64 timeout,
                      const bool temp, const QStringList &args);

private Q_SLOTS:
    void slotProcessStateChanged(QProcess::ProcessState state);
    void slotStartupRemoved(const KStartupInfoId &id, const KStartupInfoData &data);
    void slotStartupTimeout();

private:
    Q_DISABLE_COPY(KLauncherProcess);

    void sendSIStart(const qint64 timeout);
    void sendSIChange();
    void sendSIFinish();

    KStartupInfo* m_kstartupinfo;
    QTimer* m_startuptimer;
    KStartupInfoId m_kstartupinfoid;
    KStartupInfoData m_kstartupinfodata;
    bool m_temp;
    QStringList m_args;
};

// Adaptor class for interface org.kde.KLauncher
class KLauncherAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KLauncher")
public:
    KLauncherAdaptor(QObject *parent);
    ~KLauncherAdaptor();

public:
public Q_SLOTS:
    // used by plasma-desktop
    void autoStart(int phase);

    // used by plasma-desktop and klauncher itself
    void exec_blind(const QString &name, const QStringList &args);
    void cleanup();

    // used by KToolInvocation
    void setLaunchEnv(const QString &name, const QString &value);
    bool start_program(const QString &app, const QStringList &args, const QStringList &envs,
                       quint64 window, bool temp);
    bool start_program_with_workdir(const QString &app, const QStringList &args,
                                    const QStringList &envs, quint64 window, bool temp,
                                    const QString &workdir);
    bool start_service_by_storage_id(const QString &serviceName, const QStringList &urls,
                                     const QStringList &envs, quint64 window, bool temp);
    bool start_service_by_url(const QString &url, const QStringList &envs, quint64 window,
                              bool temp);

    // for debugging
#ifdef KLAUNCHER_DEBUG
    QStringList environment() const;
#endif

Q_SIGNALS:
    void autoStart0Done();
    void autoStart1Done();
    void autoStart2Done();

private Q_SLOTS:
    void slotProcessFinished(int exitcode);

private:
    QString findExe(const QString &app) const;
    bool startProgram(const QString &app, const QStringList &args, const QStringList &envs,
                      const quint64 window, const bool temp, const QString &workdir,
                      const qint64 timeout, const KService::Ptr kservice = KService::Ptr(nullptr));

    QProcessEnvironment m_environment;
    qint64 m_startuptimeout;
    QList<KLauncherProcess*> m_processes;
    QStringList m_autostart;
};

#endif // KLAUNCHER_ADAPTOR_H
