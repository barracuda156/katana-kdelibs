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

#ifndef KPLASMAJOBTRACKER_H
#define KPLASMAJOBTRACKER_H

#include <kdeui_export.h>
#include <kjobtrackerinterface.h>

class KJob;
class KPlasmaJobTrackerPrivate;

/**
 * The interface to implement to track the progresses of a job.
 */
class KDEUI_EXPORT KPlasmaJobTracker : public KJobTrackerInterface
{
    Q_OBJECT

public:
    KPlasmaJobTracker(QObject *parent = nullptr);
    virtual ~KPlasmaJobTracker();

    virtual void registerJob(KJob *job);
    virtual void unregisterJob(KJob *job);

protected Q_SLOTS:
    virtual void finished(KJob *job);
    virtual void suspended(KJob *job);
    virtual void resumed(KJob *job);
    virtual void description(KJob *job, const QString &title,
                             const QPair<QString, QString> &field1,
                             const QPair<QString, QString> &field2);
    virtual void infoMessage(KJob *job, const QString &plain, const QString &rich);
    virtual void percent(KJob *job, unsigned long percent);

private:
    KPlasmaJobTrackerPrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_slotStopRequested(const QString &name))
};

#endif // KPLASMAJOBTRACKER_H
