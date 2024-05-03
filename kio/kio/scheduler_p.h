// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       Waldo Bastian <bastian@kde.org>

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

#ifndef KIO_SCHEDULER_P_H
#define KIO_SCHEDULER_P_H

#include "kio/job.h"
#include "kio/jobclasses.h"
#include "kio/slaveinterface_p.h"

#include <QMutex>
#include <QTimer>
#include <QList>

namespace KIO {

    /**
     * This class manages KIO slaves for the jobs
     *
     * @see KIO::SlaveInterface
     * @see KIO::Job
     **/
    class Scheduler : public QObject
    {
        Q_OBJECT
    public:
        Scheduler(QObject *parent = nullptr);
        ~Scheduler();

        /**
         * Register @p job with the scheduler.
         * @param job the job to register
         */
        void doJob(KIO::SimpleJob *job);

        /**
         * Stop the execution of a job.
         * @param job the job to cancel
         */
        void cancelJob(KIO::SimpleJob *job);

        /**
         * Called when a job is done.
         * @param job the finished job
         * @param slave the slave that executed the @p job
         */
        void jobFinished(KIO::SimpleJob *job, KIO::SlaveInterface *slave);

        void reparseSlaveConfiguration();

        static Scheduler *self();

    private Q_SLOTS:
        void slotSlaveDied(KIO::SlaveInterface *slave);
        void slotStartJob();
        void slotCheckSlaves();

    private:
        Q_DISABLE_COPY(Scheduler)

        QMutex m_mutex;
        QList<KIO::SlaveInterface*> m_slaves;
        QList<KIO::SimpleJob*> m_jobs;
        QTimer m_jobtimer;
        QTimer m_idletimer;
        bool m_initdone;
        QString m_charset;
        QString m_languages;
        QString m_useragent;
};

}
#endif
