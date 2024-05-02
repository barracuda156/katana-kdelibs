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

static const int s_jobtimeout = 100; // ms

namespace KIO
{

K_GLOBAL_STATIC(Scheduler, kScheduler)

Scheduler* Scheduler::self()
{
    return kScheduler;
}

Scheduler::Scheduler(QObject *parent)
    : QObject(parent),
    m_initdone(false)
{
    setObjectName("scheduler");
    connect(&m_jobtimer, SIGNAL(timeout()), this, SLOT(slotStartJob()));
}

Scheduler::~Scheduler()
{
    const int jobscount = m_jobs.size();
    if (jobscount > 0) {
        kWarning(7006) << "there are jobs" << jobscount;
    }
    const int slavescount = m_slaves.size();
    if (slavescount > 0) {
        kWarning(7006) << "there are slaves" << slavescount;
    }
}

void Scheduler::doJob(KIO::SimpleJob *job)
{
    QMutexLocker locker(&m_mutex);
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    m_jobs.insert(jobPriv->m_schedPrio, job);
    kDebug(7006) << "queued job" << job->url();
    if (!m_jobtimer.isActive()) {
        m_jobtimer.start(s_jobtimeout);
    }
}

void Scheduler::cancelJob(KIO::SimpleJob *job)
{
    kDebug(7006) << "canceling job" << job->url();
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    jobFinished(job, jobPriv->m_slave);
}

void Scheduler::jobFinished(KIO::SimpleJob *job, KIO::SlaveInterface *slave)
{
    QMutexLocker locker(&m_mutex);
    kDebug(7006) << "job finished" << job->url();
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    if (slave) {
        slave->disconnect(job);
        slave->kill();
        slave->deref();
    }
    m_slaves.removeAll(slave);
    jobPriv->m_slave = nullptr;
    m_jobs.removeAll(job);
}

void Scheduler::reparseSlaveConfiguration()
{
    QMutexLocker locker(&m_mutex);
    kDebug(7006) << "reparsing slave configuration";
    m_initdone = false;
    foreach (KIO::SlaveInterface* slave, m_slaves) {
        slave->send(CMD_REPARSECONFIGURATION, QByteArray());
    }
}

void Scheduler::slotSlaveDied(KIO::SlaveInterface *slave)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(slave);
    kDebug(7006) << "slave died" << slave->pid() << slave->protocol();
    slave->kill();
    m_slaves.removeAll(slave);
    locker.unlock();
    slave->deref(); // deletes slave
}

void Scheduler::slotStartJob()
{
    QMutexLocker locker(&m_mutex);
    if (m_jobs.size() < 1) {
        kDebug(7006) << "no pending jobs";
        m_jobtimer.stop();
        return;
    }
    QMutableListIterator<KIO::SimpleJob*> iter(m_jobs);
    while (iter.hasNext()) {
        KIO::SimpleJob* job = iter.next();
        const KUrl url = job->url();
        const QString protocol = url.protocol();
        const QString host = url.host();

        KIO::SlaveInterface* slave = nullptr;
        const int maxSlaves = KProtocolInfo::maxSlaves(protocol);
        const int maxSlavesPerHost = KProtocolInfo::maxSlavesPerHost(protocol);
        int slaveForProtoCounter = 0;
        int slaveForHostCounter = 0;
        foreach (KIO::SlaveInterface* itslave, m_slaves) {
            if (!itslave->isAlive()) {
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
            kDebug(7006) << "slave protocol limit reached" << protocol << slaveForProtoCounter << maxSlaves;
            break;
        }
        if (maxSlavesPerHost > 0 && slaveForHostCounter >= maxSlavesPerHost) {
            kDebug(7006) << "slave host limit reached" << protocol << slaveForHostCounter << maxSlavesPerHost;
            break;
        }

        if (!slave || slave->host() != host) {
            int error = 0;
            QString errortext;
            slave = SlaveInterface::createSlave(protocol, url, error, errortext);
            if (!slave) {
                kError(7006) << "could not create slave" << errortext;
                job->slotError(error, errortext);
                return;
            }
            kDebug(7006) << "created slave" << protocol << slave->pid();
            m_slaves.append(slave);
            QObject::connect(
                slave, SIGNAL(slaveDied(KIO::SlaveInterface*)),
                Scheduler::self(), SLOT(slotSlaveDied(KIO::SlaveInterface*))
            );
        } else {
            kDebug(7006) << "using exisitng slave" << slave->pid();
        }

        MetaData configData;
        KSharedConfig::Ptr config = KProtocolManager::config();
        if (config) {
            configData += config->entryMap("<default>");
        }
        config = KSharedConfig::openConfig(KProtocolInfo::config(protocol), KConfig::NoGlobals);
        if (config) {
            configData += config->entryMap(host);
        }
        if (protocol.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
            if (!m_initdone) {
                m_initdone = true;
                m_language = KProtocolManager::acceptLanguagesHeader();
                m_charsets = QString::fromLatin1(QTextCodec::codecForLocale()->name()).toLower();
                m_useragent = KProtocolManager::defaultUserAgent();
            }

            // these might have already been set so check first to make sure that metadata is not
            // overriden
            if (configData["Languages"].isEmpty()) {
                configData["Languages"] = m_language;
            }
            if (configData["Charsets"].isEmpty()) {
                configData["Charsets"] = m_charsets;
            }
            if (configData["UserAgent"].isEmpty()) {
                configData["UserAgent"] = m_useragent;
            }
        }

        slave->setConfig(configData);
        slave->setHost(host);

        KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
        jobPriv->m_slave = slave;
        kDebug(7006) << "starting queued job" << jobPriv << jobPriv->m_slave->pid();
        iter.remove();
        jobPriv->start(jobPriv->m_slave);
    }
}

}

#include "moc_scheduler_p.cpp"
