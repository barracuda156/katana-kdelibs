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

#include "kplasmajobtracker.h"

#include <QDBusInterface>
#include <QDBusConnectionInterface>
#include <kglobal.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>
#include <kjob.h>
#include <kdebug.h>

static QString kJobID(const KJob *job)
{
    return QString::number(quintptr(job), 16);
}

class KPlasmaJobTrackerPrivate
{
public:
    KPlasmaJobTrackerPrivate();

    void _k_slotStopRequested(const QString &name);

    QMap<KJob*, QVariantMap> jobs;
    QDBusInterface interface;
};

KPlasmaJobTrackerPrivate::KPlasmaJobTrackerPrivate()
    : interface("org.kde.plasma-desktop", "/JobTracker", "org.kde.JobTracker", QDBusConnection::sessionBus())
{
    QDBusConnectionInterface* sessionIface = QDBusConnection::sessionBus().interface();
    if (!sessionIface->isServiceRegistered("org.kde.plasma-desktop")) {
        kError() << "The service org.kde.plasma-desktop is still not registered";
    } else {
        kDebug() << "Plasma job tracker registered";
    }
}

void KPlasmaJobTrackerPrivate::_k_slotStopRequested(const QString &name)
{
    kDebug() << "job stop requested" << name;
    foreach (KJob* job, jobs.keys()) {
        const QString jobid = kJobID(job);
        if (jobid == name) {
            job->kill(KJob::EmitResult);
            break;
        }
    }
}


KPlasmaJobTracker::KPlasmaJobTracker(QObject *parent)
    : KJobTrackerInterface(parent),
    d(new KPlasmaJobTrackerPrivate())
{
    if (d->interface.isValid()) {
        connect(
            &d->interface, SIGNAL(stopRequested(QString)),
            this, SLOT(_k_slotStopRequested(QString))
        );
    }
}

KPlasmaJobTracker::~KPlasmaJobTracker()
{
    delete d;
}

void KPlasmaJobTracker::registerJob(KJob *job)
{
    if (d->jobs.contains(job)) {
        kWarning() << "atempting to register the same job twice" << job;
        return;
    }

    const KComponentData componentData = KGlobal::mainComponent();
    QString appName = job->property("appName").toString();
    if (appName.isEmpty()) {
        appName = componentData.aboutData()->programName();
    }
    QString appIconName = job->property("appIconName").toString();
    if (appIconName.isEmpty()) {
        appIconName = componentData.aboutData()->programIconName();
    }
    if (appIconName.isEmpty()) {
        appIconName = componentData.aboutData()->appName();
    }
    const QString destUrl = job->property("destUrl").toString();

    const QString jobid = kJobID(job);
    QVariantMap jobdata;
    jobdata.insert("infoMessage", QString());
    jobdata.insert("appName", appName); // currently not used
    jobdata.insert("appIconName", appIconName);
    jobdata.insert("labelName0", QString());
    jobdata.insert("labelName1", QString());
    jobdata.insert("label0", QString());
    jobdata.insert("label1", QString());
    jobdata.insert("destUrl", destUrl);
    jobdata.insert("error", QString());
    jobdata.insert("percentage", 0);
    jobdata.insert("state", "running");
    jobdata.insert("killable", bool(job->capabilities() & KJob::Killable));
    d->jobs.insert(job, jobdata);
    d->interface.call("addJob", jobid);
    d->interface.call("updateJob", jobid, jobdata);

    kDebug() << "registerd job" << jobid << jobdata;
    KJobTrackerInterface::registerJob(job);
}

void KPlasmaJobTracker::unregisterJob(KJob *job)
{
    KJobTrackerInterface::unregisterJob(job);

    if (!d->jobs.contains(job)) {
        return;
    }

    // both finished() and unregistrJob will be called, either does it
    kDebug() << "unregisterd job" << kJobID(job);
    finished(job);
    d->jobs.remove(job);
}

void KPlasmaJobTracker::finished(KJob *job)
{
    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("state", "stopped");
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job finished" << jobid;
    d->jobs.remove(job);
}

void KPlasmaJobTracker::suspended(KJob *job)
{
    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("state", "suspended");
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job suspended" << jobid;
}

void KPlasmaJobTracker::resumed(KJob *job)
{
    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("state", "running");
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job resumed" << jobid;
}

void KPlasmaJobTracker::description(KJob *job, const QString &title,
                                      const QPair<QString, QString> &field1,
                                      const QPair<QString, QString> &field2)
{
    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("labelName0", field1.first);
    jobdata.insert("label0", field1.second);
    jobdata.insert("labelName1", field2.first);
    jobdata.insert("label1", field2.second);
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job description" << jobid << field1 << field2;
}

void KPlasmaJobTracker::infoMessage(KJob *job, const QString &plain, const QString &rich)
{
    Q_UNUSED(rich);

    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("infoMessage", plain);
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job info message" << jobid << plain << rich;
}

void KPlasmaJobTracker::percent(KJob *job, unsigned long percent)
{
    if (!d->jobs.contains(job)) {
        return;
    }

    const QString jobid = kJobID(job);
    QVariantMap jobdata = d->jobs.value(job);
    jobdata.insert("percent", qulonglong(percent));
    d->interface.call("updateJob", jobid, jobdata);
    kDebug() << "job percent" << jobid << percent;
}

#include "moc_kplasmajobtracker.cpp"
