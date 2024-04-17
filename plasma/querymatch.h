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

#ifndef PLASMA_QUERYMATCH_H
#define PLASMA_QUERYMATCH_H

#include <plasma/plasma_export.h>

#include <QAction>
#include <QIcon>
#include <QString>
#include <QVariant>
#include <QSharedDataPointer>

namespace Plasma
{

class RunnerContext;
class AbstractRunner;
class QueryMatchPrivate;

/**
 * @class QueryMatch plasma/querymatch.h <Plasma/QueryMatch>
 *
 * @short A match returned by an AbstractRunner in response to a given
 * RunnerContext.
 */
class PLASMA_EXPORT QueryMatch
{
    public:
        /**
         * Constructs a match associated with a given runner.
         *
         * @param runner the runner this match belongs to
         */
        explicit QueryMatch(AbstractRunner *runner);

        /**
         * Copy constructor
         */
        QueryMatch(const QueryMatch &other);

        ~QueryMatch();

        QueryMatch &operator=(const QueryMatch &other);
        bool operator==(const QueryMatch &other) const;
        bool operator!=(const QueryMatch &other) const;
        bool operator<(const QueryMatch &other) const;

        /**
         * @return the runner associated with this action
         */
        AbstractRunner* runner() const;

        /**
         * @return true if the match is valid and can therefore be run,
         *         an invalid match does not have an associated AbstractRunner
         */
        bool isValid() const;

        /**
         * Sets the relevance of this action for the search
         * it was created for.
         *
         * @param relevance a number between 0 and 1.
         */
        void setRelevance(qreal relevance);

        /**
         * The relevance of this action to the search. By default,
         * the relevance is 1.
         *
         * @return a number between 0 and 1
         */
        qreal relevance() const;

        /**
         * Requests this match to activae using the given context
         *
         * @param context the context to use in conjunction with this run
         *
         * @sa AbstractRunner::run
         */
        void run(const RunnerContext &context) const;

        /**
         * Sets data to be used internally by the associated
         * AbstractRunner.
         *
         * When set, it is also used to form
         * part of the id() for this match. If that is innapropriate
         * as an id, the runner may generate its own id and set that
         * with setId(const QString&) directly after calling setData
         */
        void setData(const QVariant &data);

        /**
         * @return the data associated with this match; usually runner-specific
         */
        QVariant data() const;

        /**
         * Sets the id for this match; useful if the id does not
         * match data().toString(). The id must be unique to all
         * matches from this runner, and should remain constant
         * for the same query for best results.
         *
         * @param id the new identifying string to use to refer
         *           to this entry
         */
        void setId(const QString &id);

        /**
         * @ruetnr a string that can be used as an ID for this match,
         * even between different queries. It is based in part
         * on the source of the match (the AbstractRunner) and
         * distinguishing information provided by the runner,
         * ensuring global uniqueness as well as consistency
         * between query matches.
         */
        QString id() const;

        /**
         * Sets the main title text for this match; should be short
         * enough to fit nicely on one line in a user interface
         *
         * @param text the text to use as the title
         */
        void setText(const QString &text);

        /**
         * @return the title text for this match
         */
        QString text() const;

        /**
         * Sets the descriptive text for this match; can be longer
         * than the main title text
         *
         * @param text the text to use as the description
         */
        void setSubtext(const QString &text);

        /**
         * @return the descriptive text for this match
         */
        QString subtext() const;

        /**
         * Sets the icon associated with this match
         *
         * @param icon the icon to show along with the match
         */
        void setIcon(const QIcon &icon);

        /**
         * @return the icon for this match
         */
        QIcon icon() const;

        /**
         * Sets whether or not this match can be activited
         *
         * @param enable true if the match is enabled and therefore runnable
         */
        void setEnabled(bool enable);

        /**
         * @return true if the match is enabled and therefore runnable, otherwise false
         */
        bool isEnabled() const;

        /**
         * The current action.
         */
        QAction* selectedAction() const;

        /**
         * Sets the selected action
         */
        void setSelectedAction(QAction *action);

    private:
        QSharedDataPointer<QueryMatchPrivate> d;
};

}

#endif
