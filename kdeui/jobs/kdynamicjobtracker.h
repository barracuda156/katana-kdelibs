/*
 * Copyright 2008 by Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
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

#ifndef KDYNAMICJOBTRACKER_H
#define KDYNAMICJOBTRACKER_H

#include <kdeui_export.h>
#include <kjobtrackerinterface.h>

/**
 * This class implements a simple job tracker which registers any job to the KWidgetJobTracker if a
 * plasma isn't available on the DBus, or to the KPlasmaJobTracker, if a plasma is available. This
 * way, we have the old dialogs as fallback when the user doesn't use a plasma applet.
 */
class KDEUI_EXPORT KDynamicJobTracker : public KJobTrackerInterface
{
    Q_OBJECT
public:
    /**
     * Creates a new KDynamicJobTracker
     *
     * @param parent the parent of this object.
     */
    KDynamicJobTracker(QObject *parent = 0);

    /**
     * Destroys this KDynamicJobTracker
     */
    virtual ~KDynamicJobTracker();

public Q_SLOTS:
    /**
     * Register a new job in this tracker.
     *
     * @param job the job to register
     */
    virtual void registerJob(KJob *job);

    /**
     * Unregister a job from the tracker it was registered to.
     *
     * @param job the job to unregister
     */
    virtual void unregisterJob(KJob *job);

private:
    class Private;
    Private *const d;
};

#endif // KDYNAMICJOBTRACKER_H
