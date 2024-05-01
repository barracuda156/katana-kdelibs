/* This file is part of the KDE libraries
   Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                      Waldo Bastian <bastian@kde.org>
   Copyright (C) 2009, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

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

#include "scheduler_p.h"
#include "slaveinterface_p.h"
#include "job_p.h"
#include "kprotocolmanager.h"
#include "kprotocolinfo.h"
#include "kdebug.h"

#include <QTextCodec>

// Slaves may be idle for a certain time (1 minute) before they are killed.
static const int s_idleSlaveLifetime = 1 * 60;

namespace KIO
{

/********************************* SessionData ****************************/
SessionData::SessionData()
    : initDone(false)
{
}

void SessionData::configDataFor(MetaData &configData, const QString &proto)
{
    if (proto.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
        if (!initDone) {
            reset();
        }

        // these might have already been set so check first to make sure that we do not trumpt
        // settings sent by apps or end-user.
        if (configData["Languages"].isEmpty()) {
            configData["Languages"] = language;
        }
        if (configData["Charsets"].isEmpty()) {
            configData["Charsets"] = charsets;
        }
        if (configData["UserAgent"].isEmpty()) {
            configData["UserAgent"] = KProtocolManager::defaultUserAgent();
        }
    }
}

void SessionData::reset()
{
    initDone = true;
    language = KProtocolManager::acceptLanguagesHeader();
    charsets = QString::fromLatin1(QTextCodec::codecForLocale()->name()).toLower();
    KProtocolManager::reparseConfiguration();
}

K_GLOBAL_STATIC(Scheduler, kScheduler)

Scheduler* Scheduler::self()
{
    return kScheduler;
}

Scheduler::Scheduler(QObject *parent)
    : QObject(parent)
{
    setObjectName("scheduler");
    connect(&m_jobtimer, SIGNAL(timeout()), this, SLOT(slotStartJob()));
    connect(&m_idletimer, SIGNAL(timeout()), this, SLOT(slotCheckSlaves()));
}

Scheduler::~Scheduler()
{
}

void Scheduler::doJob(KIO::SimpleJob *job)
{
    QMutexLocker locker(&m_mutex);
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    jobPriv->m_schedSerial = 1;
    m_jobs.append(job);
    kDebug(7006) << "Scheduler: queued job" << job->url();
    if (!m_jobtimer.isActive()) {
        m_jobtimer.start(100);
    }
    if (!m_idletimer.isActive()) {
        m_idletimer.start(3000);
    }
}

void Scheduler::cancelJob(KIO::SimpleJob *job)
{
    QMutexLocker locker(&m_mutex);
    kDebug(7006) << "Scheduler: canceling job" << job->url();
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    KIO::SlaveInterface* slave = jobPriv->m_slave;
    if (slave) {
        slave->disconnect(job);
        slave->kill();
    }
    m_slaves.removeAll(slave);
    jobPriv->m_schedSerial = 0; // this marks the job as unscheduled again
    jobPriv->m_slave = nullptr;
    m_jobs.removeAll(job);
}

void Scheduler::jobFinished(KIO::SimpleJob *job, KIO::SlaveInterface *slave)
{
    QMutexLocker locker(&m_mutex);
    kDebug(7006) << "Scheduler: job finished" << job->url();
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    if (slave) {
        slave->disconnect(job);
        slave->setIdle();
    }
    jobPriv->m_schedSerial = 0; // this marks the job as unscheduled again
    jobPriv->m_slave = nullptr;
    m_jobs.removeAll(job);
}

void Scheduler::reparseSlaveConfiguration()
{
    QMutexLocker locker(&m_mutex);
    KProtocolManager::reparseConfiguration();
    m_sessionData.reset();
    foreach (KIO::SlaveInterface* slave, m_slaves) {
        slave->send(CMD_REPARSECONFIGURATION, QByteArray());
    }
}

void Scheduler::slotSlaveDied(KIO::SlaveInterface *slave)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(slave);
    kDebug(7006) << "Scheduler: slave died" << slave->pid() << slave->protocol();
    slave->kill();
    m_slaves.removeAll(slave);
    locker.unlock();
    slave->deref(); // Delete slave
}

void Scheduler::slotStartJob()
{
    QMutexLocker locker(&m_mutex);
    if (m_jobs.size() < 1) {
        kDebug(7006) << "Scheduler: no pending jobs";
        m_jobtimer.stop();
        return;
    }
    QMutableListIterator<KIO::SimpleJob*> iter(m_jobs);
    while (iter.hasNext()) {
        KIO::SimpleJob* job = iter.next();
        const KUrl url = job->url();
        const QString protocol = url.protocol();
        KUrl hosturl = url;
        const bool islocalfile = hosturl.isLocalFile();
        if (!islocalfile) {
            hosturl.setPath(QString());
        }
        hosturl.setQuery(QString());
        hosturl.setFragment(QString());
        const QString host = hosturl.url();

        KIO::SlaveInterface* slave = nullptr;
        const int maxSlaves = KProtocolInfo::maxSlaves(protocol);
        const int maxSlavesPerHost = KProtocolInfo::maxSlavesPerHost(protocol);
        int slaveForProtoCounter = 0;
        int slaveForHostCounter = 0;
        foreach (KIO::SlaveInterface* itslave, m_slaves) {
            if (!itslave->isAlive()) {
                continue;
            }
            if (itslave->protocol() == protocol && itslave->idleTime() > 0) {
                slave = itslave;
                continue;
            }
            if (itslave->protocol() == protocol) {
                slaveForProtoCounter++;
            }
            if (itslave->host() == host) {
                slaveForHostCounter++;
            }
        }
        if (maxSlaves > 0 && slaveForProtoCounter >= maxSlaves) {
            kDebug(7006) << "Scheduler: slave protocol limit reached" << protocol << slaveForProtoCounter << maxSlaves;
            break;
        }
        if (!islocalfile && maxSlavesPerHost > 0 && slaveForHostCounter >= maxSlavesPerHost) {
            kDebug(7006) << "Scheduler: slave host limit reached" << protocol << slaveForHostCounter << maxSlavesPerHost;
            break;
        }

        if (!slave || slave->host() != host) {
            int error = 0;
            QString errortext;
            slave = SlaveInterface::createSlave(protocol, url, error, errortext);
            if (!slave) {
                kError(7006) << "Scheduler:  could not create slave:" << errortext;
                job->slotError(error, errortext);
                return;
            }
            kDebug(7006) << "Scheduler: created slave" << protocol << slave->pid();
            m_slaves.append(slave);
            QObject::connect(
                slave, SIGNAL(slaveDied(KIO::SlaveInterface*)),
                Scheduler::self(), SLOT(slotSlaveDied(KIO::SlaveInterface*))
            );
        } else {
            kDebug(7006) << "Scheduler: using exisitng slave" << slave->pid();
        }

        MetaData configData;
        KSharedConfig::Ptr config = KProtocolManager::config();
        if (config) {
            configData += config->entryMap("<default>");
        }
        config = KSharedConfig::openConfig(KProtocolInfo::config(protocol), KConfig::NoGlobals);
        if (!islocalfile) {
            configData += config->entryMap(url.host());
        }
        m_sessionData.configDataFor(configData, protocol);
        slave->setConfig(configData);
        slave->setHost(host);

        KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
        jobPriv->m_slave = slave;
        kDebug(7006) << "Scheduler: starting queued job" << jobPriv << jobPriv->m_slave->pid();
        iter.remove();
        jobPriv->start(jobPriv->m_slave);
    }
}

void Scheduler::slotCheckSlaves()
{
    // TODO:
}

}

#include "moc_scheduler_p.cpp"
