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
#include <QThreadPool>

#include <kdebug.h>
#include <kplugininfo.h>
#include <kservicetypetrader.h>
#include <kstandarddirs.h>

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
        prepped(false),
        allRunnersPrepped(false),
        teardownRequested(false)
    {
        threadPool = new QThreadPool();

        matchChangeTimer.setSingleShot(true);

        QObject::connect(&matchChangeTimer, SIGNAL(timeout()), q, SLOT(matchesChanged()));
        QObject::connect(&context, SIGNAL(matchesChanged()), q, SLOT(scheduleMatchesChanged()));
    }

    ~RunnerManagerPrivate()
    {
        kDebug() << "waiting for runner jobs";
        threadPool->waitForDone();
        delete threadPool;
    }

    void scheduleMatchesChanged()
    {
        matchChangeTimer.start(100);
    }

    void matchesChanged()
    {
        emit q->matchesChanged(context.matches());
    }

    void loadRunners()
    {
        KPluginInfo::List offers = RunnerManager::listRunnerInfo();

        QSet<AbstractRunner *> deadRunners;
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

    AbstractRunner *loadInstalledRunner(const KService::Ptr service)
    {
        if (!service) {
            return 0;
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
            if (prepped) {
                emit runner->prepare();
            }
        }

        return runner;
    }

    void checkTearDown()
    {
        //kDebug() << prepped << teardownRequested << threadPool->activeThreadCount();

        if (!prepped || !teardownRequested) {
            return;
        }

        if (threadPool->activeThreadCount() <= 0) {
            if (allRunnersPrepped) {
                foreach (AbstractRunner *runner, runners) {
                    emit runner->teardown();
                }

                allRunnersPrepped = false;
            }

            emit q->queryFinished();

            prepped = false;
            teardownRequested = false;
        }
    }

    RunnerManager *q;
    RunnerContext context;
    QTimer matchChangeTimer;
    QHash<QString, AbstractRunner*> runners;
    QThreadPool *threadPool;
    QStringList allowedRunners;
    bool prepped;
    bool allRunnersPrepped;
    bool teardownRequested;
};

/*****************************************************
*  RunnerManager::Public class
*
*****************************************************/
RunnerManager::RunnerManager(QObject *parent)
    : QObject(parent),
      d(new RunnerManagerPrivate(this))
{
    int maxThreads = QThread::idealThreadCount();
    if (maxThreads < 0) {
        maxThreads = 4;
    }
    kDebug() << "limiting runner threads to" << maxThreads;
    //This entry allows to define a hard upper limit independent of the number of processors.
    d->threadPool->setMaxThreadCount(maxThreads);
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
    KPluginInfo description(service);
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

QList<AbstractRunner *> RunnerManager::runners() const
{
    return d->runners.values();
}

QString RunnerManager::runnerName(const QString &id) const
{
    if (runner(id)) {
        return runner(id)->name();
    }
    return QString();
}

RunnerContext* RunnerManager::searchContext() const
{
    return &d->context;
}

QList<QueryMatch> RunnerManager::matches() const
{
    return d->context.matches();
}

void RunnerManager::run(const QString &id)
{
    run(d->context.match(id));
}

void RunnerManager::run(const QueryMatch &match)
{
    if (!match.isEnabled()) {
        return;
    }
    d->context.run(match);
}

QList<QAction*> RunnerManager::actionsForMatch(const QueryMatch &match)
{
    AbstractRunner *runner = match.runner();
    if (runner) {
        return runner->actionsForMatch(match);
    }
    return QList<QAction*>();
}

QMimeData* RunnerManager::mimeDataForMatch(const QString &id) const
{
    return mimeDataForMatch(d->context.match(id));
}

QMimeData* RunnerManager::mimeDataForMatch(const QueryMatch &match) const
{
    AbstractRunner *runner = match.runner();
    QMimeData *mimeData;
    if (runner && QMetaObject::invokeMethod(
            runner,
            "mimeDataForMatch", Qt::DirectConnection,
            Q_RETURN_ARG(QMimeData*, mimeData),
            Q_ARG(const Plasma::QueryMatch *, &match)
    )) {
        return mimeData;
    }

    return 0;
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

void RunnerManager::setupMatchSession()
{
    d->teardownRequested = false;

    if (d->prepped) {
        return;
    }

    d->prepped = true;
    foreach (AbstractRunner *runner, d->runners) {
#ifdef MEASURE_PREPTIME
        QTime t;
        t.start();
#endif
        emit runner->prepare();
#ifdef MEASURE_PREPTIME
        kDebug() << t.elapsed() << runner->name();
#endif
    }

    d->allRunnersPrepped = true;
}

void RunnerManager::matchSessionComplete()
{
    if (!d->prepped) {
        return;
    }

    d->teardownRequested = true;
    d->checkTearDown();
}

void RunnerManager::launchQuery(const QString &untrimmedTerm)
{
    setupMatchSession();
    QString term = untrimmedTerm.trimmed();

    if (term.isEmpty()) {
        reset();
        return;
    }

    if (d->context.query() == term) {
        // already searching for this!
        return;
    }

    if (d->runners.isEmpty()) {
        d->loadRunners();
    }

    reset();
    // kDebug() << "runners searching for" << term;
    d->context.setQuery(term);

    foreach (Plasma::AbstractRunner *runner, d->runners) {
        if ((runner->ignoredTypes() & d->context.type()) == 0) {
            FindMatchesJob *job = new FindMatchesJob(runner, &d->context);
            d->threadPool->start(job);
        }
    }
}

QString RunnerManager::query() const
{
    return d->context.query();
}

void RunnerManager::reset()
{
    d->threadPool->waitForDone(3000);

    d->context.reset();
}

} // Plasma namespace

#include "moc_runnermanager.cpp"
