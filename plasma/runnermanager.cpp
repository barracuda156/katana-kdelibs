/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
 *   Copyright (C) 2007, 2009 Ryan P. Bitanga <ryan.bitanga@gmail.com>
 *   Copyright (C) 2008 Jordi Polo <mumismo@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "runnermanager.h"
#include "private/runnerjobs_p.h"
#include "querymatch.h"

#include <QTimer>
#include <QCoreApplication>

#include "kplugininfo.h"
#include "kservicetypetrader.h"
#include "kstandarddirs.h"
#include "kthreadpool.h"
#include "kdebug.h"

// #define MEASURE_PREPTIME

namespace Plasma
{

/*****************************************************
*  RunnerManager::Private class
*
*****************************************************/
class RunnerManagerPrivate
{
public:
    RunnerManagerPrivate(RunnerManager *parent)
        : q(parent),
        threadPool(nullptr),
        finishedTimer(nullptr)
    {
        threadPool = new KThreadPool(q);
        finishedTimer = new QTimer(q);
        finishedTimer->setInterval(500);

        QObject::connect(finishedTimer, SIGNAL(timeout()), q, SLOT(_k_checkFinished()));
        QObject::connect(&context, SIGNAL(matchesChanged()), q, SLOT(_k_matchesChanged()));
    }

    ~RunnerManagerPrivate()
    {
        kDebug() << "waiting for runner jobs";
        threadPool->waitForDone();
        delete threadPool;
    }

    void _k_matchesChanged()
    {
        emit q->matchesChanged(context.matches());
    }

    void _k_checkFinished()
    {
        // kDebug() << threadPool->activeThreadCount();
        if (threadPool->activeThreadCount() <= 0) {
            finishedTimer->stop();
            emit q->queryFinished();
        }
    }

    void loadRunners()
    {
        KPluginInfo::List offers = RunnerManager::listRunnerInfo();

        QSet<AbstractRunner*> deadRunners;
        QMutableListIterator<KPluginInfo> it(offers);
        while (it.hasNext()) {
            const KPluginInfo& description = it.next();
            const QString tryExec = description.property("TryExec").toString();
            // kDebug() << "TryExec is" << tryExec;
            if (!tryExec.isEmpty() && KStandardDirs::findExe(tryExec).isEmpty()) {
                kWarning() << "Runner executable missing:" << tryExec;
                continue;
            }

            const QString runnerName = description.pluginName();
            const bool loaded = runners.contains(runnerName);
            const bool selected = allowedRunners.contains(runnerName);

            // kDebug() << description.isPluginEnabled() << loaded << selected;
            if (selected) {
                if (!loaded) {
                    AbstractRunner *runner = loadInstalledRunner(description.service());

                    if (runner) {
                        runners.insert(runnerName, runner);
                    }
                }
            } else if (loaded) {
                deadRunners.insert(runners.take(runnerName));
                kDebug() << "Removing runner:" << runnerName;
            }
        }

        if (!deadRunners.isEmpty()) {
             qDeleteAll(deadRunners);
        }

        kDebug() << "All runners loaded, total:" << runners.count();
    }

    AbstractRunner* loadInstalledRunner(const KService::Ptr service)
    {
        if (!service) {
            return nullptr;
        }

        QVariantList args;
        args << service->storageId();
        QString error;
        AbstractRunner *runner = service->createInstance<AbstractRunner>(q, args, &error);
        if (!runner) {
            kWarning() << "Failed to load runner:" << service->name() << ". error reported:" << error;
        } else {
            kDebug() << "================= loading runner:" << service->name() << "=================";
            QMetaObject::invokeMethod(runner, "init");
        }

        return runner;
    }

    static QThread::Priority threadPriority(const AbstractRunner::Priority priority)
    {
        switch (priority) {
            case AbstractRunner::LowestPriority: {
                return QThread::LowestPriority;
            }
            case AbstractRunner::LowPriority: {
                return QThread::LowPriority;
            }
            case AbstractRunner::NormalPriority: {
                return QThread::NormalPriority;
            }
            case AbstractRunner::HighPriority: {
                return QThread::HighPriority;
            }
            case AbstractRunner::HighestPriority: {
                return QThread::HighestPriority;
            }
        }
        kWarning() << "unhandled runner priority" << priority;
        return QThread::InheritPriority;
    }

    RunnerManager *q;
    KThreadPool *threadPool;
    QTimer *finishedTimer;
    QStringList allowedRunners;
    QHash<QString, AbstractRunner*> runners;
    RunnerContext context;
};

/*****************************************************
*  RunnerManager::Public class
*
*****************************************************/
RunnerManager::RunnerManager(QObject *parent)
    : QObject(parent),
    d(new RunnerManagerPrivate(this))
{
}

RunnerManager::~RunnerManager()
{
    delete d;
}

void RunnerManager::setAllowedRunners(const QStringList &runners)
{
    d->allowedRunners = runners;
    d->loadRunners();
}

QStringList RunnerManager::allowedRunners() const
{
    return d->allowedRunners;
}

void RunnerManager::loadRunner(const KService::Ptr service)
{
    const KPluginInfo description(service);
    const QString runnerName = description.pluginName();
    if (!runnerName.isEmpty() && !d->runners.contains(runnerName)) {
        AbstractRunner *runner = d->loadInstalledRunner(service);
        if (runner) {
            d->runners.insert(runnerName, runner);
        }
    }
}

AbstractRunner* RunnerManager::runner(const QString &name) const
{
    if (d->runners.isEmpty()) {
        d->loadRunners();
    }
    return d->runners.value(name, nullptr);
}

QList<AbstractRunner*> RunnerManager::runners() const
{
    return d->runners.values();
}

RunnerContext* RunnerManager::searchContext() const
{
    return &d->context;
}

QList<QueryMatch> RunnerManager::matches() const
{
    return d->context.matches();
}

QList<QAction*> RunnerManager::actionsForMatch(const QueryMatch &match)
{
    AbstractRunner *runner = match.runner();
    if (runner) {
        return runner->actionsForMatch(match);
    }
    return QList<QAction*>();
}

QMimeData* RunnerManager::mimeDataForMatch(const QueryMatch &match) const
{
    AbstractRunner *runner = match.runner();
    if (runner) {
        return runner->mimeDataForMatch(match);
    }
    return nullptr;
}

KPluginInfo::List RunnerManager::listRunnerInfo(const QString &parentApp)
{
    QString constraint;
    if (parentApp.isEmpty()) {
        constraint.append("not exist [X-KDE-ParentApp]");
    } else {
        constraint.append("[X-KDE-ParentApp] == '").append(parentApp).append("'");
    }

    KService::List offers = KServiceTypeTrader::self()->query("Plasma/Runner", constraint);
    return KPluginInfo::fromServices(offers);
}

void RunnerManager::launchQuery(const QString &untrimmedTerm)
{
    reset();

    QString term = untrimmedTerm.trimmed();

    if (term.isEmpty()) {
        return;
    }

    if (d->context.query() == term) {
        // already searching for this!
        return;
    }

    if (d->runners.isEmpty()) {
        d->loadRunners();
    }

    // kDebug() << "runners searching for" << term;
    d->context.setQuery(term);

    foreach (Plasma::AbstractRunner *runner, d->runners) {
        if ((runner->ignoredTypes() & d->context.type()) == 0) {
            FindMatchesJob *job = new FindMatchesJob(runner, &d->context);
            d->threadPool->start(job, RunnerManagerPrivate::threadPriority(runner->priority()));
        }
    }

    d->finishedTimer->start();
}

QString RunnerManager::query() const
{
    return d->context.query();
}

void RunnerManager::reset()
{
    d->threadPool->waitForDone();
    d->context.reset();
}

} // Plasma namespace

#include "moc_runnermanager.cpp"
