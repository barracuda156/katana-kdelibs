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

#include "klauncher_adaptor.h"
#include "krun.h"
#include "kstandarddirs.h"
#include "kautostart.h"
#include "kshell.h"
#include "kconfiggroup.h"
#include "kmessagebox.h"
#include "kdesktopfile.h"
#include "kmimetype.h"
#include "kmimetypetrader.h"
#include "kprotocolmanager.h"
#include "kio/netaccess.h"
#include "kio/udsentry.h"
#include "kdebug.h"

#include <QDir>
#include <QApplication>
#include <QThread>

// for reference:
// https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html

static const int s_eventstime = 250;
static const int s_sleeptime = 50;
// NOTE: keep in sync with:
// kde-workspace/kwin/effects/startupfeedback/startupfeedback.cpp
// kde-workspace/kcontrol/launch/kcmlaunch.cpp
static const int s_startuptimeout = 10; // 10sec
// klauncher is one of the last processes to quit in a session so 5sec for each child process is
// more than enough
static const qint64 s_processtimeout = 5000; // 5sec
static const int s_deletedelay = 3000; // 3sec

static inline void removeTemp(const bool temp, const QStringList &args)
{
    if (temp) {
        foreach (const QString &arg, args) {
            if (QFile::exists(arg)) {
                kDebug() << "removing temporary file" << arg;
                QFile::remove(arg);
            }
        }
    }
}

static inline void showError(const QString &error, const quint64 window)
{
    KMessageBox::errorWId(static_cast<WId>(window), error);
}

// TODO: QWidget::find() does not find external windows
static inline QWidget* findWindow(const quint64 window)
{
    if (!window) {
        return nullptr;
    }
    return QWidget::find(static_cast<WId>(window));
}

KLauncherProcess::KLauncherProcess(QObject *parent)
    : QProcess(parent),
    m_kstartupinfo(nullptr),
    m_startuptimer(nullptr),
    m_window(0),
    m_temp(false)
{
    connect(
        this, SIGNAL(stateChanged(QProcess::ProcessState)),
        this, SLOT(slotProcessStateChanged(QProcess::ProcessState))
    );
}

KLauncherProcess::~KLauncherProcess()
{
    removeTemp(m_temp, m_args);
}

void KLauncherProcess::setupProcess(const QString &appexe, const QStringList &args,
                                    const quint64 window, const KService::Ptr kservice,
                                    const qint64 timeout, const bool temp,
                                    const KLauncherDownloads &downloaded)
{
    Q_ASSERT(m_kstartupinfoid.none() == true);
    m_appexe = appexe;
    m_args = args;
    m_window = window;
    m_temp = temp;
    m_downloaded = downloaded;
    QByteArray startupwmclass;
    if (KRun::checkStartupNotify(kservice.data(), &startupwmclass)) {
        m_kstartupinfoid.initId(KStartupInfo::createNewStartupId());
        kDebug() << "setting up ASN for" << kservice->entryPath() << m_kstartupinfoid.id();
        m_kstartupinfodata.setHostname();
        m_kstartupinfodata.setBin(QFileInfo(m_appexe).fileName());
        m_kstartupinfodata.setDescription(i18n("Launching %1", kservice->name()));
        m_kstartupinfodata.setIcon(kservice->icon());
        m_kstartupinfodata.setApplicationId(kservice->entryPath());
        m_kstartupinfodata.setWMClass(startupwmclass);
        QProcessEnvironment processenv = QProcess::processEnvironment();
        processenv.insert(QString::fromLatin1("DESKTOP_STARTUP_ID"), m_kstartupinfoid.id());
        QProcess::setProcessEnvironment(processenv);
        sendSIStart(timeout);
    } else {
        kDebug() << "no ASN for" << m_appexe;
    }
    foreach (const QString &download, m_downloaded.keys()) {
        const QDateTime downloadlastmodified = QFileInfo(download).lastModified();
        kDebug() << "current last modified for" << download << "is" << downloadlastmodified;
        m_lastmodified.insert(download, downloadlastmodified);
    }
}

void KLauncherProcess::slotProcessStateChanged(QProcess::ProcessState state)
{
    kDebug() << "process state changed" << m_appexe << state;
    if (state == QProcess::Starting && !m_kstartupinfoid.none()) {
        m_kstartupinfodata.addPid(QProcess::pid());
        sendSIChange();
    } else if (state == QProcess::NotRunning) {
        if (!m_kstartupinfoid.none()) {
            sendSIFinish();
        }
        if (exitCode() != 0) {
            kError() << "finished with error" << m_appexe;
            const QByteArray processerror = readAllStandardError();
            if (processerror.isEmpty()) {
                showError(i18n("Application exited abnormally: %1", m_appexe), m_window);
            } else {
                KMessageBox::detailedErrorWId(
                    static_cast<WId>(m_window),
                    i18n("Application exited abnormally: %1", m_appexe),
                    QString::fromLocal8Bit(processerror.constData(), processerror.size())
                );
            }
        }
        if (!m_downloaded.isEmpty()) {
            foreach (const QString &download, m_downloaded.keys()) {
                const QDateTime downloadlastmodified = QFileInfo(download).lastModified();
                kDebug() << "current last modified for" << download << "is" << downloadlastmodified;
                if (downloadlastmodified != m_lastmodified.value(download)) {
                    // TODO: maybe ask
                    const KUrl uploadurl = m_downloaded.value(download);
                    kDebug() << "uploading" << download << "to" << uploadurl;
                    if (!KIO::NetAccess::upload(download, uploadurl, findWindow(m_window))) {
                        kWarning() << "could not upload" << download;
                    }
                }
            }
        }
        QTimer::singleShot(s_deletedelay, this, SLOT(deleteLater()));
    }
}

void KLauncherProcess::slotStartupRemoved(const KStartupInfoId &kstartupinfoid,
                                          const KStartupInfoData &kstartupinfodata)
{
    if (m_kstartupinfoid.none()) {
        return;
    }

    kDebug() << "startup removed" << kstartupinfoid.id() << m_kstartupinfoid.id();
    if (kstartupinfoid.id() == m_kstartupinfoid.id() || kstartupinfodata.is_pid(QProcess::pid())) {
        kDebug() << "startup done for process" << m_appexe;
        sendSIFinish();
    }
}

void KLauncherProcess::slotStartupTimeout()
{
    kWarning() << "timed out while waiting for process" << m_appexe;
    sendSIFinish();
}

void KLauncherProcess::sendSIStart(const qint64 timeout)
{
    if (m_kstartupinfoid.none()) {
        return;
    }

    kDebug() << "sending ASN start for" << m_kstartupinfodata.bin();
    m_kstartupinfo = new KStartupInfo(this);
    connect(
        m_kstartupinfo, SIGNAL(gotRemoveStartup(KStartupInfoId,KStartupInfoData)),
        this, SLOT(slotStartupRemoved(KStartupInfoId,KStartupInfoData))
    );

    m_startuptimer = new QTimer(this);
    m_startuptimer->setSingleShot(true);
    m_startuptimer->setInterval(timeout);
    connect(m_startuptimer, SIGNAL(timeout()), this, SLOT(slotStartupTimeout()));
    m_startuptimer->start();

    KStartupInfo::sendStartup(m_kstartupinfoid, m_kstartupinfodata);
}

void KLauncherProcess::sendSIChange()
{
    if (m_kstartupinfoid.none()) {
        return;
    }
    kDebug() << "sending ASN change for" << m_appexe;
    KStartupInfo::sendChange(m_kstartupinfoid, m_kstartupinfodata);
}

void KLauncherProcess::sendSIFinish()
{
    if (m_kstartupinfoid.none()) {
        return;
    }
    kDebug() << "sending ASN finish for" << m_appexe;
    KStartupInfo::sendFinish(m_kstartupinfoid, m_kstartupinfodata);
    m_kstartupinfoid = KStartupInfoId();
    m_kstartupinfodata = KStartupInfoData();
    if (m_startuptimer) {
        m_startuptimer->stop();
    }
}

KLauncherAdaptor::KLauncherAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent),
    m_startuptimeout(0)
{
    m_environment = QProcessEnvironment::systemEnvironment();

    // TODO: config watch
    KConfig klauncherconfig("klaunchrc", KConfig::NoGlobals);
    KConfigGroup kconfiggroup = klauncherconfig.group("BusyCursorSettings");
    const int busytimeout = kconfiggroup.readEntry("Timeout", s_startuptimeout);
    m_startuptimeout = (qMax(busytimeout, 1) * 1000);
}

KLauncherAdaptor::~KLauncherAdaptor()
{
    cleanup();
}

void KLauncherAdaptor::autoStart(int phase)
{
    if (m_autostart.isEmpty()) {
        kDebug() << "finding autostart desktop files" << phase;
        m_autostart = KGlobal::dirs()->findAllResources(
            "autostart",
            QString::fromLatin1("*.desktop"),
            KStandardDirs::NoDuplicates
        );
    }

    kDebug() << "autostart phase" << phase;
    foreach(const QString &it, m_autostart) {
        kDebug() << "checking autostart" << it;
        KAutostart kautostart(it);
        if (kautostart.startPhase() != phase) {
            continue;
        }
        if (!kautostart.autostarts(QString::fromLatin1("KDE"), KAutostart::CheckAll)) {
            kDebug() << "not autostarting" << it;
            continue;
        }
        QStringList programandargs = KShell::splitArgs(kautostart.command());
        if (programandargs.isEmpty()) {
            kWarning() << "could not process autostart" << it;
            continue;
        }
        const QString program = programandargs.takeFirst();
        startDetached(program, programandargs);
    }
    switch (phase) {
        case 0: {
            emit autoStart0Done();
            break;
        }
        case 1: {
            emit autoStart1Done();
            break;
        }
        case 2: {
            emit autoStart2Done();
            break;
        }
        default: {
            kWarning() << "invalid startup phase";
            break;
        }
    }
}

void KLauncherAdaptor::cleanup()
{
    kDebug() << "terminating processes" << m_processes.size();
    while (!m_processes.isEmpty()) {
        KLauncherProcess* process = m_processes.takeLast();
        disconnect(process, 0, this, 0);
        process->terminate();
        if (!process->waitForFinished(s_processtimeout)) {
            kWarning() << "process still running" << process->pid();
            // SIGKILL is non-ignorable
            process->kill();
        }
    }
}

bool KLauncherAdaptor::start_program(const QString &app, const QStringList &args,
                                    const QStringList &envs, quint64 window, bool temp)
{
    return start_program_with_workdir(app, args, envs, window, temp, QDir::homePath());
}

bool KLauncherAdaptor::start_program_with_workdir(const QString &app, const QStringList &args,
                                                 const QStringList &envs, quint64 window,
                                                 bool temp, const QString &workdir)
{
    return startProgram(app, args, envs, window, temp, workdir, m_startuptimeout);
}

void KLauncherAdaptor::setLaunchEnv(const QString &name, const QString &value)
{
    if (name.isEmpty()) {
        kWarning() << "attempting to set empty environment variable to" << value;
        return;
    }
    kDebug() << "setting environment variable" << name << "to" << value;
    m_environment.insert(name, value);
}

bool KLauncherAdaptor::start_service_by_storage_id(const QString &serviceName,
                                                   const QStringList &urls,
                                                   const QStringList &envs, quint64 window,
                                                   bool temp)
{
    KService::Ptr kservice = KService::serviceByStorageId(serviceName);
    if (!kservice) {
        kError() << "invalid service" << serviceName;
        showError(i18n("Invalid service: %1", serviceName), window);
        removeTemp(temp, urls);
        return false;
    }
    if (urls.size() > 1 && !kservice->allowMultipleFiles()) {
        kWarning() << "service does not support multiple files" << serviceName;
        bool result = true;
        foreach (const QString &url, urls) {
            if (!start_service_by_storage_id(serviceName, QStringList() << url, envs, window, temp)) {
                // if one fails then it is not exactly a success
                result = false;
            }
        }
        return result;
    }
    QStringList programandargs = KRun::processDesktopExec(*kservice, urls);
    if (programandargs.isEmpty()) {
        kError() << "could not process service" << kservice->entryPath();
        showError(i18n("Could not process service: %1", serviceName), window);
        removeTemp(temp, urls);
        return false;
    }
    QString programworkdir = kservice->path();
    if (programworkdir.isEmpty()) {
        programworkdir = QDir::homePath();
    }
    const QString program = programandargs.takeFirst();
    const QString kserviceexec = kservice->exec();
    KLauncherDownloads downloaded;
    if (!kserviceexec.contains(QLatin1String("%u")) && !kserviceexec.contains(QLatin1String("%U"))) {
        kDebug() << "service does not support remote" << serviceName;
        foreach (const QString &url, urls) {
            const KUrl realurl = KUrl(url);
            if (!realurl.isLocalFile()) {
                // remote URLs should not be passed along with temporary files
                Q_ASSERT(!temp);
                kDebug() << "downloading" << url;
                QString urldestination;
                const QString prettyurl = realurl.prettyUrl();
                if (!KIO::NetAccess::download(realurl, urldestination, findWindow(window))) {
                    kError() << "could not download" << prettyurl;
                    showError(i18n("Could not download URL: %1", prettyurl), window);
                    removeTemp(temp, urls);
                    removeTemp(true, downloaded.keys());
                    return false;
                }
                kDebug() << "downloaded" << prettyurl << "to" << urldestination;
                downloaded.insert(urldestination, realurl);
                // URLs may not be unique
                int indexofurl = programandargs.indexOf(url);
                while (indexofurl != -1) {
                    programandargs.replace(indexofurl, urldestination);
                    indexofurl = programandargs.indexOf(url);
                }
            }
        }
        temp = (temp || !downloaded.isEmpty());
    }
    kDebug() << "starting" << kservice->entryPath() << urls;
    return startProgram(program, programandargs, envs, window, temp, programworkdir, m_startuptimeout, kservice, downloaded);
}

bool KLauncherAdaptor::start_service_by_url(const QString &url, const QStringList &envs,
                                            quint64 window, bool temp)
{
    const KUrl realurl = KUrl(url);
    QString urlmimetype;
    if (realurl.isLocalFile()) {
        KMimeType::Ptr kmimetype = KMimeType::findByUrl(realurl);
        if (kmimetype) {
            urlmimetype = kmimetype->name();
        }
    } else {
        KIO::UDSEntry kioudsentry;
        if (!KIO::NetAccess::stat(realurl, kioudsentry, findWindow(window))) {
            kWarning() << "could not stat URL for MIME type" << url;
            urlmimetype = KProtocolManager::defaultMimetype(realurl);
        } else {
            urlmimetype = kioudsentry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
        }
        if (urlmimetype.isEmpty()) {
            // NOTE: scheme handlers are not valid MIME type but are used as such (e.g. in .desktop
            // files) despite the fact that none of the scheme handlers actually has a entry in the
            // shared MIME database
            const QString servicemime = QString::fromLatin1("x-scheme-handler/") + realurl.protocol();
            KService::Ptr schemeservice = KMimeTypeTrader::self()->preferredService(servicemime);
            if (schemeservice) {
                urlmimetype = servicemime;
            }
        }
    }
    if (urlmimetype.isEmpty()) {
        kError() << "invalid MIME type for path" << url;
        showError(i18n("Could not determine the MIME type of: %1", url), window);
        removeTemp(temp, QStringList() << url);
        return false;
    }
    kDebug() << "MIME type of" << url << "is" << urlmimetype;
    if (KRun::isExecutable(urlmimetype)) {
        kDebug() << "execuable file" << url;
        // safety second for some
        if (urlmimetype == QLatin1String("application/x-desktop") && KDesktopFile::isAuthorizedDesktopFile(url)) {
            kDebug() << "desktop file is authorized" << url;
            return start_service_by_storage_id(url, QStringList(), envs, window, temp);
        }
        KMessageBox::sorryWId(
            static_cast<WId>(window),
            i18n("The file <tt>%1</tt> is an executable program.<br/>For safety it will not be started.", realurl.prettyUrl())
        );
        removeTemp(temp, QStringList() << url);
        return false;
    }
    KService::Ptr kservice = KMimeTypeTrader::self()->preferredService(urlmimetype);
    if (!kservice) {
        kDebug() << "invalid service for MIME type" << urlmimetype;
        KUrl::List urllist;
        urllist << realurl;
        return KRun::displayOpenWithDialog(urllist, findWindow(window), temp);
    }
    return start_service_by_storage_id(kservice->entryPath(), QStringList() << url, envs, window, temp);
}

#ifdef KLAUNCHER_DEBUG
QStringList KLauncherAdaptor::environment() const
{
    return m_environment.toStringList();
}
#endif

void KLauncherAdaptor::slotProcessFinished(int exitcode)
{
    KLauncherProcess* process = qobject_cast<KLauncherProcess*>(sender());
    kDebug() << "process finished" << process << exitcode;
    m_processes.removeOne(process);
}

QString KLauncherAdaptor::findExe(const QString &app) const
{
    if (QDir::isAbsolutePath(app)) {
        if (!QFile::exists(app)) {
            // return empty string if it does not exists (like KStandardDirs::findExe())
            return QString();
        }
        return app;
    }
    const QString environmentpath = m_environment.value(QString::fromLatin1("PATH"), QString());
    return KStandardDirs::findExe(app, environmentpath);
}

void KLauncherAdaptor::startDetached(const QString &name, const QStringList &args)
{
    const QString appexe = findExe(name);
    if (appexe.isEmpty()) {
        kWarning() << "could not find" << name;
        return;
    }

    const QStringList envlist = m_environment.toStringList();
    kDebug() << "blind starting" << appexe << args << envlist;
    const QString envexe = findExe("env");
    if (envexe.isEmpty()) {
        kWarning() << "env program not found";
        QProcess::startDetached(appexe, args);
        return;
    }

    QStringList envargs = envlist;
    envargs += appexe;
    envargs += args;
    QProcess::startDetached(envexe, envargs);
}

bool KLauncherAdaptor::startProgram(const QString &app, const QStringList &args, const QStringList &envs,
                                    const quint64 window, const bool temp, const QString &workdir,
                                    const qint64 timeout, const KService::Ptr kservice,
                                    const KLauncherDownloads &downloaded)
{
    const QString appexe = findExe(app);
    if (appexe.isEmpty()) {
        kError() << "could not find" << app;
        showError(i18n("Could not find the application: %1", app), window);
        removeTemp(temp, args);
        return false;
    }

    KLauncherProcess* process = new KLauncherProcess(this);
    m_processes.append(process);
    QProcessEnvironment processenv = m_environment;
    foreach (const QString &env, envs) {
        const int equalindex = env.indexOf(QLatin1Char('='));
        if (equalindex <= 0) {
            kWarning() << "invalid environment variable" << env;
            continue;
        }
        const QString environmentvar = env.mid(0, equalindex);
        const QString environmentvalue = env.mid(equalindex + 1, env.size() - equalindex - 1);
        kDebug() << "adding to environment" << environmentvar << environmentvalue;
        processenv.insert(environmentvar, environmentvalue);
    }
    process->setProcessEnvironment(processenv);
    process->setWorkingDirectory(workdir);
    process->setupProcess(appexe, args, window, kservice, timeout, temp, downloaded);
    kDebug() << "starting" << appexe << args << envs << workdir;
    process->start(appexe, args);
    while (process->state() == QProcess::Starting) {
        QApplication::processEvents(QEventLoop::AllEvents, s_eventstime);
        QThread::msleep(s_sleeptime);
    }
    if (process->error() == QProcess::FailedToStart || process->error() == QProcess::Crashed) {
        kError() << "could not start" << appexe;
        m_processes.removeOne(process);
        process->deleteLater();
        showError(i18n("Could not start the application: %1", app), window);
        return false;
    }
    kDebug() << "started" << appexe;
    connect(process, SIGNAL(finished(int)), this, SLOT(slotProcessFinished(int)));
    return true;
}

#include "moc_klauncher_adaptor.cpp"
