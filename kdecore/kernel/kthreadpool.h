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

#ifndef KTHREADPOOL_H
#define KTHREADPOOL_H

#include <kdecore_export.h>

#include <QObject>
#include <QThread>

class KThreadPoolPrivate;

/*!
    Thread pool class

    @since 4.24
    @warning the API is subject to change
*/
class KDECORE_EXPORT KThreadPool : public QObject
{
    Q_OBJECT

public:
    KThreadPool(QObject *parent = nullptr);
    ~KThreadPool();

    void start(QThread *thread, const QThread::Priority priority = QThread::InheritPriority);
    void waitForDone(const int timeout = 30000);

    int maxThreadCount() const;
    void setMaxThreadCount(int maxthreads);

    int activeThreadCount() const;

private:
    Q_DISABLE_COPY(KThreadPool);
    KThreadPoolPrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_slotFinished());
};

#endif // KTHREADPOOL_H
