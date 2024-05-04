/*  This file is part of the KDE libraries
 *  Copyright 2007, 2010 David Faure <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kmimeglobsfileparser_p.h"
#include "kglobal.h"
#include "kdeversion.h"
#include "kmimetype.h"
#include "kstandarddirs.h"
#include "kmimetyperepository_p.h"
#include "kdebug.h"

#include <QFile>

static bool kGlobSort(const KMimeGlobsFileParser::Glob &first, const KMimeGlobsFileParser::Glob &second)
{
    return (first.weight >= second.weight);
}

KMimeGlobsFileParser::GlobList KMimeGlobsFileParser::parseGlobs(const QStringList &globFiles)
{
    KMimeGlobsFileParser::GlobList allGlobs;
    QListIterator<QString> globIter(globFiles);
    globIter.toBack();
    // At each level, we must be able to override (not just add to) the information that we read at higher levels
    // (if glob-deleteall is used).
    while (globIter.hasPrevious()) { // global first, then local
        QString fileName = globIter.previous();
        QFile globFile(fileName);
        //kDebug() << "Now parsing" << fileName;
        parseGlobFile(&globFile, allGlobs);
    }
    // glob2 files are weight-sorted, manually sort only when more than one file is parsed
    if (globFiles.size() > 1) {
        qStableSort(allGlobs.begin(), allGlobs.end(), kGlobSort);
    }
    return allGlobs;
}

static void filterEmptyFromList(QList<QByteArray>* bytelist)
{
    QList<QByteArray>::iterator fieldsit = bytelist->begin();
    while (fieldsit != bytelist->end()) {
        if (fieldsit->isEmpty()) {
            fieldsit = bytelist->erase(fieldsit);
        } else {
            fieldsit++;
        }
    }
}

// uses a QIODevice to make unit tests possible
bool KMimeGlobsFileParser::parseGlobFile(QIODevice* file, GlobList& globs)
{
    Q_ASSERT(file);
    if (!file->open(QIODevice::ReadOnly)) {
        return false;
    }

    // for reference:
    // https://specifications.freedesktop.org/shared-mime-info-spec/latest/ar01s02.html
    // NOTE: the file is supposed to be in UTF-8 encoding however in practise no mime-type entry
    // contains non-latin1 characters
    QByteArray lastMime, lastPattern;
    QByteArray line;
    while (!file->atEnd()) {
        line = file->readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        // kDebug() << "line=" << line;

        QList<QByteArray> fields = line.split(':');
        filterEmptyFromList(&fields);

        if (fields.count() < 3) // syntax error
            continue;
        const int weight = fields.at(0).toInt();
        const QByteArray mimeTypeName = fields.at(1);
        const QByteArray pattern = fields.at(2);
        const QByteArray flagsStr = fields.value(3); // could be empty
        QList<QByteArray> flagList = flagsStr.split(',');
        filterEmptyFromList(&flagList);
        Q_ASSERT(!pattern.isEmpty());
        Q_ASSERT(!pattern.contains(':'));

        // kDebug() << " got:" << mimeTypeName << pattern;

        if (lastMime == mimeTypeName && lastPattern == pattern) {
            // Ignore duplicates, especially important for those with no flags after a line with flags:
            // 50:text/x-csrc:*.c:cs
            // 50:text/x-csrc:*.c
            continue;
        }

        const QString mimeTypeNameStr = QString::fromLatin1(mimeTypeName.constData(), mimeTypeName.size());
        if (pattern == "__NOGLOBS__") {
            // kDebug() << "removing" << mimeTypeName;
            globs.removeMime(mimeTypeNameStr);
            lastMime.clear();
        } else {
            //if (mimeTypeName == "text/plain")
            //    kDebug() << "Adding pattern" << pattern << "to mimetype" << mimeTypeName << "from globs file, with weight" << weight;
            //if (pattern.toLower() == "*.c")
            //    kDebug() << " Adding pattern" << pattern << "to mimetype" << mimeTypeName << "from globs file, with weight" << weight << "flags" << flags;
            const bool caseSensitive = flagList.contains(QByteArray("cs"));
            const QByteArray patternCs = (caseSensitive ? pattern : pattern.toLower());
            const QString patternStr = QString::fromLatin1(patternCs.constData(), patternCs.size());
            if (!globs.hasPattern(mimeTypeNameStr, patternStr)) {
                globs.append(
                    Glob(
                        mimeTypeNameStr,
                        weight,
                        patternStr,
                        caseSensitive
                    )
                );
            }
            lastMime = mimeTypeName;
            lastPattern = pattern;
        }
    }
    return true;
}
