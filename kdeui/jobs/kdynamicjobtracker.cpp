/*
 * Copyright 2008 by Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
 * Copyright 2010 Shaun Reich <shaun.reich@kdemail.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "kdynamicjobtracker.h"

#include <kplasmajobtracker.h>
#include <kwidgetjobtracker.h>
#include <kjobtrackerinterface.h>
#include <kdebug.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QMap>

struct AllTrackers
{
    KPlasmaJobTracker *plasmaTracker;
    KWidgetJobTracker *widgetTracker;
};

class KDynamicJobTracker::Private
{
public:
    Private()
        : plasmaTracker(nullptr),
        widgetTracker(nullptr)
    {
    }

    ~Private()
    {
        delete plasmaTracker;
        delete widgetTracker;
    }

    KPlasmaJobTracker *plasmaTracker;
    KWidgetJobTracker *widgetTracker;
    QMap<KJob*, AllTrackers> trackers;
};

KDynamicJobTracker::KDynamicJobTracker(QObject *parent)
    : KJobTrackerInterface(parent),
      d(new Private())
{
}

KDynamicJobTracker::~KDynamicJobTracker()
{
    delete d;
}

void KDynamicJobTracker::registerJob(KJob *job)
{
    if (!d->plasmaTracker) {
        d->plasmaTracker = new KPlasmaJobTracker();
    }

    d->trackers[job].plasmaTracker = d->plasmaTracker;
    d->trackers[job].plasmaTracker->registerJob(job);

    QDBusInterface interface("org.kde.plasma-desktop", "/JobTracker", "org.kde.JobTracker", QDBusConnection::sessionBus(), this);
    if (!interface.isValid()) {
        // create a widget tracker in addition to KPlasmaJobTracker.
        if (!d->widgetTracker) {
            d->widgetTracker = new KWidgetJobTracker();
        }
        d->trackers[job].widgetTracker = d->widgetTracker;
        d->trackers[job].widgetTracker->registerJob(job);
    }

    Q_ASSERT(d->trackers[job].plasmaTracker || d->trackers[job].widgetTracker);
}

void KDynamicJobTracker::unregisterJob(KJob *job)
{
    KPlasmaJobTracker *plasmaTracker = d->trackers[job].plasmaTracker;
    KWidgetJobTracker *widgetTracker = d->trackers[job].widgetTracker;

    if (!(widgetTracker || plasmaTracker)) {
        kWarning() << "Tried to unregister a kio job that hasn't been registered.";
        return;
    }

    if (plasmaTracker) {
        plasmaTracker->unregisterJob(job);
    }
    if (widgetTracker) {
        widgetTracker->unregisterJob(job);
    }
}

#include "moc_kdynamicjobtracker.cpp"
