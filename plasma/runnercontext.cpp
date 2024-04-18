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

#include "runnercontext.h"

#include <QMutex>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSharedData>

#include "kshell.h"
#include "kstandarddirs.h"
#include "kurl.h"
#include "kprotocolinfo.h"
#include "kdebug.h"

#include "abstractrunner.h"
#include "querymatch.h"

#define LOCK_FOR_READ(d) d->lock.lock();
#define LOCK_FOR_WRITE(d) d->lock.lock();
#define UNLOCK(d) d->lock.unlock();

namespace Plasma
{

/*
Corrects the case of the last component in a path (e.g. /usr/liB -> /usr/lib)
path: The path to be processed.
correctCasePath: The corrected-case path
mustBeDir: Tells whether the last component is a folder or doesn't matter
Returns true on success and false on error, in case of error, correctCasePath is not modified
*/
bool correctLastComponentCase(const QString &path, QString &correctCasePath, const bool mustBeDir)
{
    //kDebug() << "Correcting " << path;

    const QFileInfo pathInfo(path);
    // If the file already exists then no need to search for it.
    if (pathInfo.exists()) {
        correctCasePath = path;
        //kDebug() << "Correct path is" << correctCasePath;
        return true;
    }

    const QDir fileDir = pathInfo.dir();
    //kDebug() << "Directory is" << fileDir;

    const QString filename = pathInfo.fileName();
    //kDebug() << "Filename is" << filename;

    //kDebug() << "searching for a" << (mustBeDir ? "directory" : "directory/file");

    const QStringList matchingFilenames = fileDir.entryList(QStringList(filename),
                                          mustBeDir ? QDir::Dirs : QDir::NoFilter);

    if (matchingFilenames.empty()) {
        //kDebug() << "No matches found!!\n";
        return false;
    } else {
        /*if (matchingFilenames.size() > 1) {
            kDebug() << "Found multiple matches!!\n";
        }*/

        if (fileDir.path().endsWith(QDir::separator())) {
            correctCasePath = fileDir.path() + matchingFilenames[0];
        } else {
            correctCasePath = fileDir.path() + QDir::separator() + matchingFilenames[0];
        }

        //kDebug() << "Correct path is" << correctCasePath;
        return true;
    }
}

/*
Corrects the case of a path (e.g. /uSr/loCAL/bIN -> /usr/local/bin)
path: The path to be processed.
corrected: The corrected-case path
Returns true on success and false on error, in case of error, corrected is not modified
*/
bool correctPathCase(const QString& path, QString &corrected)
{
    // early exit check
    if (QFileInfo(path).exists()) {
        corrected = path;
        return true;
    }

    // path components
    QStringList components = path.split(QDir::separator());

    if (components.size() < 1) {
        return false;
    }

    const bool mustBeDir = components.back().isEmpty();

    //kDebug() << "Components are" << components;

    if (mustBeDir) {
        components.pop_back();
    }

    if (components.isEmpty()) {
        return true;
    }

    QString correctPath;
    const unsigned initialComponents = components.size();
    for (unsigned i = 0; i < initialComponents - 1; i ++) {
        const QString tmp = components[0] + QDir::separator() + components[1];

        if (!correctLastComponentCase(tmp, correctPath, components.size() > 2 || mustBeDir)) {
            //kDebug() << "search was not successful";
            return false;
        }

        components.removeFirst();
        components[0] = correctPath;
    }

    corrected = correctPath;
    return true;
}

class RunnerContextPrivate : public QSharedData
{
    public:
        RunnerContextPrivate(RunnerContext *context)
            : QSharedData(),
              type(RunnerContext::UnknownType),
              q(context)
        {
        }

        RunnerContextPrivate(const RunnerContextPrivate &p)
            : QSharedData(),
              type(RunnerContext::None),
              q(p.q)
        {
            //kDebug() << "boo yeah" << type;
        }

        /**
         * Determines type of query
         */
        void determineType()
        {
            // NOTE! this method must NEVER be called from
            // code that may be running in multiple threads
            // with the same data.
            type = RunnerContext::UnknownType;
            QString path = QDir::cleanPath(KShell::tildeExpand(term));

            int space = path.indexOf(' ');
            if (!KStandardDirs::findExe(path.left(space)).isEmpty()) {
                // it's a shell command if there's a space because that implies
                // that it has arguments!
                type = (space > 0) ? RunnerContext::ShellCommand :
                                     RunnerContext::Executable;
            } else {
                KUrl url(term);
                // check for a normal URL first
                // kDebug() << url << KProtocolInfo::protocolIsLocal(url.protocol()) << url.hasHost() <<
                //    url.host() << url.isLocalFile() << path << path.indexOf('/');
                const bool hasProtocol = !url.protocol().isEmpty();
                const bool isLocalProtocol = KProtocolInfo::protocolIsLocal(url.protocol());
                if (hasProtocol && 
                    ((!isLocalProtocol && url.hasHost()) ||
                     (isLocalProtocol && url.protocol() != "file"))) {
                    // we either have a network protocol with a host, so we can show matches for it
                    // or we have a non-file url that may be local so a host isn't required
                    type = RunnerContext::NetworkLocation;
                } else if (isLocalProtocol) {
                    // at this point in the game, we assume we have a path,
                    // but if a path doesn't have any slashes
                    // it's too ambiguous to be sure we're in a filesystem context
                    path = QDir::cleanPath(url.toLocalFile());
                    // kDebug() << "slash check" << path << url;
                    if (hasProtocol || ((path.indexOf('/') != -1 || path.indexOf('\\') != -1))) {
                        QString correctCasePath;
                        if (correctPathCase(path, correctCasePath)) {
                            // kDebug() << "correct case path is" << path << correctCasePath;
                            path = correctCasePath;
                            QFileInfo info(path);
                            // kDebug() << info.isSymLink() << info.isDir() << info.isFile();

                            if (info.isSymLink()) {
                                path = info.canonicalFilePath();
                                info = QFileInfo(path);
                            }
                            if (info.isDir()) {
                                type = RunnerContext::Directory;
                            } else if (info.isFile()) {
                                type = RunnerContext::File;
                            }
                        }
                    }
                }
            }

            //kDebug() << "term2type" << term << type;
        }

        void invalidate()
        {
            q = &s_dummyContext;
        }

        QMutex lock;
        QList<QueryMatch> matches;
        QString term;
        RunnerContext::Type type;
        RunnerContext * q;
        static RunnerContext s_dummyContext;
};

RunnerContext RunnerContextPrivate::s_dummyContext;

RunnerContext::RunnerContext(QObject *parent)
    : QObject(parent),
      d(new RunnerContextPrivate(this))
{
}

//copy ctor
RunnerContext::RunnerContext(const RunnerContext &other, QObject *parent)
    : QObject(parent)
{
    LOCK_FOR_READ(other.d)
    d = other.d;
    UNLOCK(other.d)
}

RunnerContext::~RunnerContext()
{
}

RunnerContext &RunnerContext::operator=(const RunnerContext &other)
{
    if (this->d == other.d) {
        return *this;
    }

    QExplicitlySharedDataPointer<Plasma::RunnerContextPrivate> oldD = d;
    LOCK_FOR_WRITE(d)
    LOCK_FOR_READ(other.d)
    d = other.d;
    UNLOCK(other.d)
    UNLOCK(oldD)
    return *this;
}

void RunnerContext::reset()
{
    LOCK_FOR_WRITE(d);
    // We will detach if we are a copy of someone. But we will reset
    // if we are the 'main' context others copied from. Resetting
    // one RunnerContext makes all the copies obsolete.

    // We need to mark the q pointer of the detached RunnerContextPrivate
    // as dirty on detach to avoid receiving results for old queries
    d->invalidate();
    UNLOCK(d);

    d.detach();

    // Now that we detached the d pointer we need to reset its q pointer

    d->q = this;

    // we still have to remove all the matches, since if the
    // ref count was 1 (e.g. only the RunnerContext is using
    // the dptr) then we won't get a copy made
    if (!d->matches.isEmpty()) {
        d->matches.clear();
    }

    d->term.clear();
    d->type = UnknownType;
    // kDebug() << "match count" << d->matches.count();
}

void RunnerContext::setQuery(const QString &term)
{
    reset();
    if (term.isEmpty()) {
        return;
    }
    d->term = term;
    d->determineType();
}

QString RunnerContext::query() const
{
    // the query term should never be set after
    // a search starts. in fact, reset() ensures this
    // and setQuery(QString) calls reset()
    return d->term;
}

RunnerContext::Type RunnerContext::type() const
{
    return d->type;
}

bool RunnerContext::isValid() const
{
    // if the qptr is dirty, it is not valid
    LOCK_FOR_READ(d)
    const bool valid = (d->q != &(d->s_dummyContext));
    UNLOCK(d)
    return valid;
}

bool RunnerContext::addMatches(const QList<QueryMatch> &matches)
{
    if (matches.isEmpty() || !isValid()) {
        // bail out if the query is empty or the qptr is dirty
        return false;
    }

    LOCK_FOR_WRITE(d)
    d->matches.append(matches);
    UNLOCK(d);
    // kDebug()<< "add matches";
    // A copied searchContext may share the d pointer,
    // we always want to sent the signal of the object that created
    // the d pointer
    emit d->q->matchesChanged();

    return true;
}

bool RunnerContext::addMatch(const QueryMatch &match)
{
    if (!isValid()) {
        // bail out if the qptr is dirty
        return false;
    }

    LOCK_FOR_WRITE(d)
    d->matches.append(match);
    UNLOCK(d);
    //kDebug()<< "added match" << match->text();
    emit d->q->matchesChanged();

    return true;
}

QList<QueryMatch> RunnerContext::matches() const
{
    LOCK_FOR_READ(d)
    QList<QueryMatch> matches = d->matches;
    UNLOCK(d);
    return matches;
}

} // Plasma namespace

#include "moc_runnercontext.cpp"
