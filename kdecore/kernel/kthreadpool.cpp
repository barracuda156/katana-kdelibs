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

#include "kthreadpool.h"
#include "kdebug.h"

#include <QMutex>
#include <QCoreApplication>
#include <QElapsedTimer>

static const int s_waittimeout = 100;
static const int s_terminatetimeout = 5000;

class KThreadPoolPrivate
{
public:
    KThreadPoolPrivate(KThreadPool *parent);

    void appendThread(QThread *thread);

    void _k_slotFinished();

    QMutex mutex;
    KThreadPool* parent;
    int maxthreads;
    int activethreadcount;
    QList<QThread*> activethreads;
    QList<QThread*> queuedthreads;
};

KThreadPoolPrivate::KThreadPoolPrivate(KThreadPool *_parent)
    : parent(_parent),
    maxthreads(QThread::idealThreadCount()),
    activethreadcount(0)
{
    kDebug() << "threads limits is" << maxthreads;
}

void KThreadPoolPrivate::appendThread(QThread *thread)
{
    activethreadcount++;
    activethreads.append(thread);
    parent->connect(
        thread, SIGNAL(finished()),
        parent, SLOT(_k_slotFinished())
    );
}

void KThreadPoolPrivate::_k_slotFinished()
{
    QMutexLocker locker(&mutex);
    QMutableListIterator<QThread*> iter(activethreads);
    while (iter.hasNext()) {
        QThread* thread = iter.next();
        if (thread->isFinished()) {
            kDebug() << "thread finished" << thread;
            iter.remove();
            activethreadcount--;
            Q_ASSERT(activethreadcount >= 0);
            thread->deleteLater();
        }
    }
    if (!queuedthreads.isEmpty()) {
        QThread* thread = queuedthreads.takeFirst();
        kDebug() << "starting thread from queue" << thread;
        appendThread(thread);
        locker.unlock();
        thread->start();
    }
}


KThreadPool::KThreadPool(QObject *parent)
    : QObject(parent ? parent : qApp),
    d(new KThreadPoolPrivate(this))
{
}

KThreadPool::~KThreadPool()
{
    waitForDone();
    delete d;
}

void KThreadPool::start(QThread *thread, const QThread::Priority priority)
{
    QMutexLocker locker(&d->mutex);
    if (d->activethreadcount >= d->maxthreads) {
        kDebug() << "too many threads active, putting thread in queue" << thread;
        d->queuedthreads.append(thread);
    } else {
        kDebug() << "starting thread" << thread;
        d->appendThread(thread);
        locker.unlock();
        thread->start(priority);
    }
}

void KThreadPool::waitForDone(const int timeout)
{
    kDebug() << "waiting for threads" << timeout;
    QMutexLocker locker(&d->mutex);
    QElapsedTimer elapsedtimer;
    elapsedtimer.start();
    QMutableListIterator<QThread*> iter(d->activethreads);
    kDebug() << "currently" << d->activethreads << "active threads";
    while ((timeout < 1 || elapsedtimer.elapsed() < timeout) && d->activethreadcount > 0) {
        iter.toFront();
        while (iter.hasNext()) {
            QThread* thread = iter.next();
            // kDebug() << "waiting for" << thread;
            disconnect(thread, 0, this, 0);
            thread->wait(s_waittimeout);
            if (thread->isFinished()) {
                kDebug() << "thread finished" << thread;
                iter.remove();
                d->activethreadcount--;
                Q_ASSERT(d->activethreadcount >= 0);
                thread->deleteLater();
            }
        }
    }
    if (d->activethreadcount > 0) {
        kWarning() << "still there are active threads" << d->activethreadcount;
        iter.toFront();
        while (iter.hasNext()) {
            QThread* thread = iter.next();
            kWarning() << "terminating" << thread;
            thread->terminate();
            thread->wait(s_terminatetimeout);
            thread->deleteLater();
        }
    }
}

int KThreadPool::maxThreadCount() const
{
    return d->maxthreads;
}

void KThreadPool::setMaxThreadCount(int maxthreads)
{
    d->maxthreads = maxthreads;
    kDebug() << "limiting threads to" << maxthreads;
}

int KThreadPool::activeThreadCount() const
{
    return d->activethreadcount;
}

#include "moc_kthreadpool.cpp"
