/*
 *   Copyright 2006-2007 Aaron Seigo <aseigo@kde.org>
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

#ifndef PLASMA_RUNNERCONTEXT_H
#define PLASMA_RUNNERCONTEXT_H

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QExplicitlySharedDataPointer>

#include <plasma/plasma_export.h>

namespace Plasma
{

class QueryMatch;
class AbstractRunner;
class RunnerContextPrivate;

/**
 * @class RunnerContext plasma/runnercontext.h <Plasma/RunnerContext>
 *
 * @short The RunnerContext class provides information related to a search,
 *        including the search term, metadata on the search term and collected
 *        matches.
 */
class PLASMA_EXPORT RunnerContext : public QObject
{
    Q_OBJECT

public:
    enum Type {
        None = 0,
        UnknownType = 1,
        Directory = 2,
        File = 4,
        NetworkLocation = 8,
        Executable = 16,
        ShellCommand = 32,
        Help = 64,
        FileSystem = Directory | File | Executable | ShellCommand
    };
    Q_DECLARE_FLAGS(Types, Type)

    explicit RunnerContext(QObject *parent = nullptr);

    /**
     * Copy constructor
     */
    RunnerContext(const RunnerContext &other, QObject *parent = nullptr);

    /**
     * Assignment operator
     * @since 4.4
     */
    RunnerContext &operator=(const RunnerContext &other);

    ~RunnerContext();

    /**
     * Resets the search term for this object. This removes all current matches in the process.
     */
    void reset();

    /**
     * Sets the query term for this object and attempts to determine
     * the type of the search.
     */
    void setQuery(const QString &term);

    /**
     * @return the current search query term.
     */
    QString query() const;

    /**
     * The type of item the search term might refer to.
     * @see Type
     */
    Type type() const;

    /**
     * @returns true if this context is no longer valid and therefore
     * matching using it should abort. Most useful as an optimization technique
     * inside of AbstractRunner subclasses in the match method, e.g.:
     *
     * while (.. a possibly large iteration) {
     *     if (!context.isValid()) {
     *         return;
     *     }
     *
     *     ... some processing ...
     * }
     *
     * While not required to be used within runners, it provies a nice way
     * to avoid unnecessary processing in runners that may run for an extended
     * period (as measured in 10s of ms) and therefore improve the user experience. 
     * @since 4.2.3
     */
    bool isValid() const;

    /**
     * Appends lists of matches to the list of matches.
     *
     * This method is thread safe and causes the matchesChanged() signal to be emitted.
     *
     * @return true if matches were added, false if matches were e.g. outdated
     */
    bool addMatches(const QList<QueryMatch> &matches);

    /**
     * Appends a match to the existing list of matches.
     *
     * If you are going to be adding multiple matches, use addMatches instead.
     *
     * @param match the match to add
     *
     * @return true if the match was added, false otherwise.
     */
    bool addMatch(const QueryMatch &match);

    /**
     * Retrieves all available matches for the current search term.
     *
     * @return a list of matches
     */
    QList<QueryMatch> matches() const;

Q_SIGNALS:
    void matchesChanged();

private:
    QExplicitlySharedDataPointer<RunnerContextPrivate> d;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Plasma::RunnerContext::Types)

#endif
