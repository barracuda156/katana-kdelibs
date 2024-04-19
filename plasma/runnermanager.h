/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
 *   Copyright (C) 2007 Ryan P. Bitanga <ryan.bitanga@gmail.com>
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

#ifndef PLASMA_RUNNERMANAGER_H
#define PLASMA_RUNNERMANAGER_H

#include <QList>
#include <QObject>
#include <QAction>

#include <kplugininfo.h>

#include <plasma/plasma_export.h>
#include "abstractrunner.h"

namespace Plasma
{
    class QueryMatch;
    class AbstractRunner;
    class RunnerContext;
    class RunnerManagerPrivate;

/**
 * @class RunnerManager plasma/runnermanager.h <Plasma/RunnerManager>
 *
 * @short The RunnerManager class is a proxy to the runners.
 */
class PLASMA_EXPORT RunnerManager : public QObject
{
    Q_OBJECT
public:
    explicit RunnerManager(QObject *parent=0);
    ~RunnerManager();

    /**
     * Finds and returns a loaded runner or null
     *
     * @param name the name of the runner
     * @return Pointer to the runner
     */
    AbstractRunner* runner(const QString &name) const;

    /**
     * @return the list of all currently loaded runners
     */
    QList<AbstractRunner*> runners() const;

    /**
     * Retrieves the current context
     *
     * @return pointer to the current context
     */
    RunnerContext *searchContext() const;

    /**
     * Retrieves all available matches found so far for the launched query
     *
     * @return List of matches
     */
    QList<QueryMatch> matches() const;

    /**
     * Retrieves the list of actions, if any, for a match
     */
    QList<QAction*> actionsForMatch(const QueryMatch &match);

    /**
     * @return the current query term
     */
    QString query() const;

    /**
     * Sets a whitelist for the plugins that can be loaded, any runners not in the list are not
     * loaded and used for matching.
     *
     * @param plugins the plugin names of allowed runners
     * @since 4.4
     */
    void setAllowedRunners(const QStringList &runners);

    /**
     * Attempts to add the AbstractRunner plugin represented by the KService passed in. Use in
     * cases where specific runners should be loaded.
     *
     * @param service the service to use to load the plugin
     * @since 4.5
     */
    void loadRunner(const KService::Ptr service);

    /**
     * @return the list of allowed plugins
     * @since 4.4
     */
    QStringList allowedRunners() const;

    /**
     * @return mime data of the specified match
     * @since 4.5
     */
    QMimeData* mimeDataForMatch(const QueryMatch &match) const;

    /**
     * Causes a reload of the configuration of all loaded runners
     */
    void reloadConfiguration();

    /**
     * Returns a list of all known Runner implementations
     *
     * @param parentApp the application to filter applets on. Uses the X-KDE-ParentApp entry (if
     *                  any) in the plugin info. The default value of QString() will result in a
     *                  list containing only applets not specifically registered to an application.
     * @return list of AbstractRunners
     * @since 4.6
     **/
    static KPluginInfo::List listRunnerInfo(const QString &parentApp = QString());

public Q_SLOTS:
    /**
     * Launch a query, this will create threads and return inmediately. When matches are found, if
     * any, matchesChanged signal is emitted.
     *
     * @param term the term we want to find matches for
     */
    void launchQuery(const QString &term);

    /**
     * Reset the current data and stops the query, will block until all active runners are
     * finished (if any).
     */
    void reset();

Q_SIGNALS:
    /**
     * Emitted each time a new match is added to the list. The signal may not be emitted if not
     * matches are found.
     */
    void matchesChanged(const QList<Plasma::QueryMatch> &matches);

    /**
     * Emitted when the query finishes.
     * @since 4.5
     */
    void queryFinished();

private:
    Q_PRIVATE_SLOT(d, void _k_checkFinished())
    Q_PRIVATE_SLOT(d, void _k_matchesChanged())

    RunnerManagerPrivate * const d;

    friend class RunnerManagerPrivate;
};

}

#endif
