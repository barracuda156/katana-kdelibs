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

#include "kplasmajobtracker.h"
#include "kwidgetjobtracker.h"
#include "kjobtrackerinterface.h"
#include "kdebug.h"

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

bool KDynamicJobTracker::registerJob(KJob *job)
{
    if (!d->plasmaTracker) {
        d->plasmaTracker = new KPlasmaJobTracker();
    }
    if (d->plasmaTracker->registerJob(job)) {
        return true;
    }
    // create a widget tracker in addition to KPlasmaJobTracker.
    if (!d->widgetTracker) {
        d->widgetTracker = new KWidgetJobTracker();
    }
    return d->widgetTracker->registerJob(job);
}

void KDynamicJobTracker::unregisterJob(KJob *job)
{
    if (d->plasmaTracker) {
        d->plasmaTracker->unregisterJob(job);
    }
    if (d->widgetTracker) {
        d->widgetTracker->unregisterJob(job);
    }
}

#include "moc_kdynamicjobtracker.cpp"
