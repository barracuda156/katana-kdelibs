/*  This file is part of the KDE libraries
 *  Copyright (C) 2006-2007, 2010 David Faure <faure@kde.org>
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

#include "kmimetype.h"
#include "kmimetyperepository_p.h"
#include <kstandarddirs.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>
#include <kdeversion.h> // KDE_MAKE_VERSION
#include <klocale.h>
#include <kstandarddirs.h>
#include <ksycoca.h>

#include <QFile>
#include <QtEndian>

#include <string.h>

extern int servicesDebugArea();

static int mimeDataBaseVersion()
{
    // shared-mime-info installs a "version" file since 0.91
    const QString versionFile = KGlobal::dirs()->findResource("xdgdata-mime", QLatin1String("version"));
    if (!versionFile.isEmpty()) {
        QFile file(versionFile);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray versionData = file.readAll().trimmed();
            const QList<QByteArray> versionParts = versionData.split('.');
            if (versionParts.size() == 3) {
                return KDE_MAKE_VERSION(versionParts.at(0).toInt(), versionParts.at(1).toInt(), versionParts.at(2).toInt());
            } else if (versionParts.size() == 2) {
                return KDE_MAKE_VERSION(versionParts.at(0).toInt(), versionParts.at(1).toInt(), 0);
            }
        }
    }

    return -1;
}

static QString fallbackParent(const QString& mimeTypeName)
{
    const QString myGroup = mimeTypeName.left(mimeTypeName.indexOf(QLatin1Char('/')));
    // All text/* types are subclasses of text/plain.
    if (myGroup == QLatin1String("text") && mimeTypeName != QLatin1String("text/plain")) {
        return QLatin1String("text/plain");
    }
    // All real-file mimetypes implicitly derive from application/octet-stream
    if (myGroup != QLatin1String("inode") &&
        // kde extensions
        myGroup != QLatin1String("all") && myGroup != QLatin1String("fonts")
        && myGroup != QLatin1String("print") && myGroup != QLatin1String("uri")
        && mimeTypeName != QLatin1String("application/octet-stream"))
    {
        return QLatin1String("application/octet-stream");
    }
    return QString();
}

// Sort them in descending order of priority
static bool mimeMagicRuleCompare(const KMimeMagicRule& lhs, const KMimeMagicRule& rhs)
{
    return lhs.priority() > rhs.priority();
}

KMimeTypeRepository * KMimeTypeRepository::self()
{
    K_GLOBAL_STATIC(KMimeTypeRepository, s_self)
    return s_self;
}

KMimeTypeRepository::KMimeTypeRepository()
    : m_mimeTypesChecked(false),
    m_useFavIcons(true),
    m_useFavIconsChecked(false),
    m_sharedMimeInfoVersion(0)
{
    parseMimeData();
    connect(KSycoca::self(), SIGNAL(databaseChanged(QStringList)), this, SLOT(parseMimeData(QStringList)));
}

void KMimeTypeRepository::parseMimeData(const QStringList &resources)
{
    if (resources.contains(QLatin1String("xdgdata-mime"))) {
        parseMimeData();
    }
}

void KMimeTypeRepository::parseMimeData()
{
    QMutexLocker locker(&m_mutex);

    const QStringList globFiles = KGlobal::dirs()->findAllResources("xdgdata-mime", QString::fromLatin1("globs2"));
    m_globs = KMimeGlobsFileParser::parseGlobs(globFiles);

    m_aliases.clear();
    const QStringList aliasFiles = KGlobal::dirs()->findAllResources("xdgdata-mime", QLatin1String("aliases"));
    Q_FOREACH(const QString& fileName, aliasFiles) {
        QFile qfile(fileName);
        // kDebug() << "Now parsing" << fileName;
        if (qfile.open(QIODevice::ReadOnly)) {
            while (!qfile.atEnd()) {
                const QByteArray line = qfile.readLine().trimmed();
                if (line.isEmpty() || line[0] == '#') {
                    continue;
                }
                const int pos = line.indexOf(' ');
                if (pos == -1) {
                    // syntax error
                    continue;
                }
                const QByteArray aliasTypeName = line.left(pos);
                const QByteArray parentTypeName = line.mid(pos+1);
                Q_ASSERT(!aliasTypeName.isEmpty());
                Q_ASSERT(!parentTypeName.isEmpty());
                const QString aliasTypeNameStr = QString::fromLatin1(aliasTypeName.constData(), aliasTypeName.size());
                const QString parentTypeNameStr = QString::fromLatin1(parentTypeName.constData(), parentTypeName.size());

                const KMimeType::Ptr realMimeType = findMimeTypeByName(aliasTypeNameStr, KMimeType::DontResolveAlias);
                if (realMimeType) {
                    // kDebug() << "Ignoring alias" << aliasTypeNameStr << "because also defined as a real mimetype";
                } else {
                    m_aliases.insert(aliasTypeNameStr, parentTypeNameStr);
                }
            }
        }
    }

    m_parents.clear();
    const QStringList subclassFiles = KGlobal::dirs()->findAllResources("xdgdata-mime", QLatin1String("subclasses"));
    // kDebug() << subclassFiles;
    Q_FOREACH(const QString &fileName, subclassFiles) {
        QFile qfile(fileName);
        // kDebug() << "Now parsing" << fileName;
        if (qfile.open(QIODevice::ReadOnly)) {
            while (!qfile.atEnd()) {
                const QByteArray line = qfile.readLine().trimmed();
                if (line.isEmpty() || line[0] == '#') {
                    continue;
                }
                const int pos = line.indexOf(' ');
                if (pos == -1) {
                    // syntax error
                    continue;
                }
                const QByteArray derivedTypeName = line.left(pos);
                const QString derivedTypeNameStr = QString::fromLatin1(derivedTypeName.constData(), derivedTypeName.size());
                KMimeType::Ptr derivedType = findMimeTypeByName(derivedTypeNameStr, KMimeType::ResolveAliases);
                if (!derivedType) {
                    kWarning() << fileName << " refers to unknown mimetype " << derivedTypeNameStr;
                } else {
                    const QByteArray parentTypeName = line.mid(pos + 1);
                    const QString parentTypeNameStr = QString::fromLatin1(parentTypeName.constData(), parentTypeName.size());
                    Q_ASSERT(!parentTypeName.isEmpty());
                    //derivedType->setParentMimeType(parentTypeNameStr);
                    m_parents[derivedTypeNameStr].append(parentTypeNameStr);
                }
            }
        }
    }

    m_magicRules.clear();
    const QStringList magicFiles = KGlobal::dirs()->findAllResources("xdgdata-mime", QLatin1String("magic"));
    // kDebug() << magicFiles;
    QListIterator<QString> magicIter( magicFiles );
    // global first, then local. Turns out it doesn't matter though.
    magicIter.toBack();
    while (magicIter.hasPrevious()) {
        const QString fileName = magicIter.previous();
        QFile magicFile(fileName);
        // kDebug() << "Now parsing " << fileName;
        if (magicFile.open(QIODevice::ReadOnly)) {
            m_magicRules += parseMagicFile(&magicFile, fileName);
        }
    }
    qSort(m_magicRules.begin(), m_magicRules.end(), mimeMagicRuleCompare);
}

KMimeType::Ptr KMimeTypeRepository::findMimeTypeByName(const QString &_name, KMimeType::FindByNameOption options) const
{
    QString name = _name;
    if (options & KMimeType::ResolveAliases) {
        name = canonicalName(name);
    }

    const QString filename = KGlobal::dirs()->findResource("xdgdata-mime", name.toLower() + QLatin1String(".xml"));
    if (filename.isEmpty()) {
        return KMimeType::Ptr(); // Not found
    }

    return KMimeType::Ptr(new KMimeType(filename, name));
}

bool KMimeTypeRepository::checkMimeTypes()
{
    // check if there are mimetypes
    const QStringList globFiles = KGlobal::dirs()->findAllResources("xdgdata-mime", QLatin1String("globs2"));
    return !globFiles.isEmpty();
}

QString KMimeTypeRepository::resolveAlias(const QString& mime) const
{
    return m_aliases.value(mime);
}

QString KMimeTypeRepository::canonicalName(const QString& mime) const
{
    QString c = resolveAlias(mime);
    if (c.isEmpty()) {
        return mime;
    }
    return c;
}

bool KMimeTypeRepository::matchFileName(const QString &filename, const QString &pattern, const Qt::CaseSensitivity cs)
{
    const int pattern_len = pattern.length();
    if (!pattern_len) {
        return false;
    }

    const int len = filename.length();
    const int starCount = pattern.count(QLatin1Char('*'));
    const int bracketIndex = pattern.indexOf(QLatin1Char('['));
    const int questionIndex = pattern.indexOf(QLatin1Char('?'));
    if (starCount == 1 && bracketIndex == -1 && questionIndex == -1) {
        if (pattern[0] == QLatin1Char('*')) {
            // Patterns like "*~", "*.extension"
            if (len + 1 < pattern_len) {
                return false;
            }
            return filename.endsWith(pattern.mid(1, pattern_len - 1), cs);
        } else if (pattern[pattern_len - 1] == QLatin1Char('*')) {
            // Patterns like "README*"
            if (len + 1 < pattern_len) {
                return false;
            }
            return filename.startsWith(pattern.mid(0, pattern_len - 1), cs);
        }
    }

    // Names without any wildcards like "README"
    if (starCount == 0 && bracketIndex == -1 && questionIndex == -1) {
        return filename.compare(pattern, cs) == 0;
    }

    // Other (quite rare) patterns, like "*.anim[1-9j]": use slow but correct method
    QRegExp rx(pattern, cs, QRegExp::Wildcard);
    return rx.exactMatch(filename);
}

QStringList KMimeTypeRepository::findFromFileName(const QString &fileName, QString *pMatchingExtension) const
{
    QStringList matchingMimeTypes;
    QString foundExt;
    int matchingPatternLength = 0;
    qint32 lastMatchedWeight = 0;

    foreach (const KMimeGlobsFileParser::Glob &glob, m_globs) {
        if (matchFileName(fileName, glob.pattern, glob.casesensitive)) {
            // Is this a lower-weight pattern than the last match? Stop here then.
            if (glob.weight < lastMatchedWeight) {
                break;
            }
            if (lastMatchedWeight > 0 && glob.weight > lastMatchedWeight) {
                // can't happen
                kWarning() << "Assumption failed; globs2 weights not sorted correctly" << glob.weight << ">" << lastMatchedWeight;
            }
            // Is this a shorter or a longer match than an existing one, or same length?
            if (glob.pattern.length() < matchingPatternLength) {
                // too short, ignore
                continue;
            } else if (glob.pattern.length() > matchingPatternLength) {
                // longer: clear any previous match (like *.bz2, when pattern is *.tar.bz2)
                matchingMimeTypes.clear();
                // remember the new "longer" length
                matchingPatternLength = glob.pattern.length();
            }
            matchingMimeTypes.append(glob.mimeType);
            lastMatchedWeight = glob.weight;
            if (glob.pattern.startsWith(QLatin1String("*."))) {
                foundExt = glob.pattern.mid(2);
            }
        }
    }

    if (pMatchingExtension) {
        *pMatchingExtension = foundExt;
    }
    return matchingMimeTypes;
}

KMimeType::Ptr KMimeTypeRepository::findFromContent(QIODevice* device, int* accuracy)
{
    Q_ASSERT(device->isOpen());
    const qint64 deviceSize = device->size();
    if (deviceSize == 0) {
        if (accuracy) {
            *accuracy = 100;
        }
        return findMimeTypeByName(QLatin1String("application/x-zerosize"));
    }
    // provide enough data for most rules (there are exceptions which require twice as much tho)
    const qint64 dataNeeded = qMin(deviceSize, (qint64) 16384);
    QByteArray beginning(dataNeeded, '\0');
    if (!device->seek(0) || device->read(beginning.data(), dataNeeded) == -1) {
        return defaultMimeTypePtr(); // don't bother detecting unreadable file
    }

    // Apply magic rules
    Q_FOREACH ( const KMimeMagicRule& rule, m_magicRules ) {
        if (rule.match(device, deviceSize, beginning)) {
            if (accuracy) {
                *accuracy = rule.priority();
            }
            return findMimeTypeByName(rule.mimetype());
        }
    }

    // Do fallback code so that we never return 0
    // Nothing worked, check if the file contents looks like binary or text
    if (!KMimeType::isBufferBinaryData(beginning)) {
        if (accuracy) {
            *accuracy = 5;
        }
        return findMimeTypeByName(QLatin1String("text/plain"));
    }
    if (accuracy) {
        *accuracy = 0;
    }
    return defaultMimeTypePtr();
}

QStringList KMimeTypeRepository::parents(const QString& mime) const
{
    QStringList parents = m_parents.value(mime);
    if (parents.isEmpty()) {
        const QString myParent = fallbackParent(mime);
        if (!myParent.isEmpty()) {
            parents.append(myParent);
        }
    }

    return parents;
}

static char readNumber(qint64 &value, QIODevice *file)
{
    char ch;
    while (file->getChar(&ch)) {
        if (ch < '0' || ch > '9') {
            return ch;
        }
        value = (10 * value + ch - '0');
    }
    // eof
    return '\0';
}


#define MAKE_LITTLE_ENDIAN16(val) val = (quint16)(((quint16)(val) << 8)|((quint16)(val) >> 8))

#define MAKE_LITTLE_ENDIAN32(val) \
   val = (((quint32)(val) & 0xFF000000U) >> 24) | \
         (((quint32)(val) & 0x00FF0000U) >> 8) | \
         (((quint32)(val) & 0x0000FF00U) << 8) | \
         (((quint32)(val) & 0x000000FFU) << 24)

QList<KMimeMagicRule> KMimeTypeRepository::parseMagicFile(QIODevice *file, const QString &fileName) const
{
    QList<KMimeMagicRule> rules;
    QByteArray header = file->read(12);
    if (header.size() != 12 || ::memcmp(header.constData(), "MIME-Magic\0\n", 12) != 0) {
        kWarning() << "Invalid magic file " << fileName << " starts with " << header;
        return rules;
    }
    QList<KMimeMagicMatch> matches; // toplevel matches (indent==0)
    int priority = 50;
    QByteArray mimeTypeName;

    static const int sharedmimeinfover = mimeDataBaseVersion();
    static const int sharedmimeinfo200 = KDE_MAKE_VERSION(2, 0, 0);

    Q_FOREVER {
        char ch = '\0';
        bool chOk = file->getChar(&ch);

        if (!chOk || ch == '[') {
            // Finish previous section
            if (!mimeTypeName.isEmpty()) {
                // workaround for:
                // https://gitlab.freedesktop.org/xdg/shared-mime-info/-/issues/144
                if (sharedmimeinfover <= sharedmimeinfo200 && mimeTypeName == "audio/x-mod") {
                    kDebug() << "Ignoring audio/x-mod magic rules";
                } else {
                    const QString mimeTypeNameStr = QString::fromLatin1(mimeTypeName.constData(), mimeTypeName.size());
                    rules.append(KMimeMagicRule(mimeTypeNameStr, priority, matches));
                }
                matches.clear();
                mimeTypeName.clear();
            }
            if (file->atEnd())
                break; // done

            // Parse new section
            const QByteArray line = file->readLine();
            const int pos = line.indexOf(':');
            if (pos == -1) {
                // syntax error
                kWarning() << "Syntax error in " << mimeTypeName << " ':' not present in section name";
                break;
            }
            priority = line.left(pos).toInt();
            mimeTypeName = line.mid(pos+1);
            mimeTypeName = mimeTypeName.left(mimeTypeName.length()-2); // remove ']\n'
            // kDebug() << "New rule for " << mimeTypeName << " with priority " << priority;
        } else {
            // Parse line in the section
            // [ indent ] ">" start-offset "=" value
            //   [ "&" mask ] [ "~" word-size ] [ "+" range-length ] "\n"
            qint64 indent = 0;
            if (ch != '>') {
                indent = ch - '0';
                ch = readNumber(indent, file);
                if (ch != '>') {
                    kWarning() << "Invalid magic file " << fileName << " '>' not found, got " << ch << " at pos " << file->pos();
                    break;
                }
            }

            KMimeMagicMatch match;
            match.m_rangeStart = 0;
            ch = readNumber(match.m_rangeStart, file);
            if (ch != '=') {
                kWarning() << "Invalid magic file " << fileName << " '=' not found";
                break;
            }

            qint16 lengthBuffer;
            if (file->read(reinterpret_cast<char*>(&lengthBuffer), 2) != 2) {
                break;
            }
            const qint16 valueLength = qFromBigEndian(lengthBuffer);
            // kDebug() << "indent=" << indent << " rangeStart=" << match.m_rangeStart
            //          << " valueLength=" << valueLength;

            match.m_data.resize(valueLength);
            if (file->read(match.m_data.data(), valueLength) != valueLength) {
                break;
            }

            match.m_rangeLength = 1;
            bool invalidLine = false;

            if (!file->getChar(&ch)) {
                break;
            }

            qint64 wordSize = 1;
            Q_FOREVER {
                // We get 'ch' before coming here, or as part of the parsing in each case below.
                switch (ch) {
                    case '\n': {
                        break;
                    }
                    case '&': {
                        match.m_mask.resize(valueLength);
                        if (file->read(match.m_mask.data(), valueLength) != valueLength)
                            invalidLine = true;
                        if (!file->getChar(&ch))
                            invalidLine = true;
                        break;
                    }
                    case '~': {
                        wordSize = 0;
                        ch = readNumber(wordSize, file);
                        // kDebug() << "wordSize=" << wordSize;
                        break;
                    }
                    case '+': {
                        // Parse range length
                        match.m_rangeLength = 0;
                        ch = readNumber(match.m_rangeLength, file);
                        if (ch == '\n') {
                            break;
                        }
                        // fall-through intended
                    }
                    default: {
                        // "If an unknown character is found where a newline is expected
                        // then the whole line should be ignored (there will be no binary
                        // data after the new character, so the next line starts after the
                        // next "\n" character). This is for future extensions.", says spec
                        while (ch != '\n' && !file->atEnd()) {
                            file->getChar(&ch);
                        }
                        invalidLine = true;
                        kDebug() << "invalid line - garbage found - ch=" << ch;
                        break;
                    }
                }
                if (ch == '\n' || invalidLine) {
                    break;
                }
            }
            if (!invalidLine) {
                // Finish match, doing byte-swapping on little endian hosts
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                if (wordSize > 1) {
                    // kDebug() << "data before swapping: " << match.m_data;;
                    if ((wordSize != 2 && wordSize != 4) || (valueLength % wordSize != 0)) {
                         // invalid word size
                        continue;
                    }
                    char* data = match.m_data.data();
                    char* mask = match.m_mask.data();
                    for (int i = 0; i < valueLength; i += wordSize) {
                        if (wordSize == 2) {
                            MAKE_LITTLE_ENDIAN16(*((quint16 *)data + i));
                        } else if (wordSize == 4) {
                            MAKE_LITTLE_ENDIAN32(*((quint32 *)data + i));
                        }
                        if (!match.m_mask.isEmpty()) {
                            if (wordSize == 2) {
                                MAKE_LITTLE_ENDIAN16( *((quint16 *) mask + i));
                            } else if (wordSize == 4) {
                                MAKE_LITTLE_ENDIAN32(*((quint32 *)mask + i));
                            }
                        }
                    }
                    // kDebug() << "data after swapping: " << match.m_data;
                }
#endif
                // Append match at the right place depending on indent:
                if (indent == 0) {
                    matches.append(match);
                } else {
                    KMimeMagicMatch* m = &matches.last();
                    Q_ASSERT(m);
                    for (int i = 1 /* nothing to do for indent==1 */; i < indent; ++i) {
                        m = &m->m_subMatches.last();
                        Q_ASSERT(m);
                    }
                    m->m_subMatches.append(match);
                }
            }
        }
    }
    return rules;
}

void KMimeTypeRepository::checkEssentialMimeTypes()
{
    if (m_mimeTypesChecked) {
        // already done
        return;
    }
    // must be done before building mimetypes
    m_mimeTypesChecked = true;

    // no shared-mime-info installed ?
    if (!checkMimeTypes()) {
        kError() << "could not find shared-mime-info";
        return;
    }

    QStringList missingMimeTypes;
    if (!KMimeType::mimeType(QLatin1String("inode/directory"))) {
        missingMimeTypes.append(QLatin1String("inode/directory"));
    }
    if (!KMimeType::mimeType(QLatin1String("inode/blockdevice"))) {
        missingMimeTypes.append(QLatin1String("inode/blockdevice"));
    }
    if (!KMimeType::mimeType(QLatin1String("inode/chardevice"))) {
        missingMimeTypes.append(QLatin1String("inode/chardevice"));
    }
    if (!KMimeType::mimeType(QLatin1String("inode/socket"))) {
        missingMimeTypes.append(QLatin1String("inode/socket"));
    }
    if (!KMimeType::mimeType(QLatin1String("inode/fifo"))) {
        missingMimeTypes.append(QLatin1String("inode/fifo"));
    }
    if (!KMimeType::mimeType(QLatin1String("application/x-shellscript"))) {
        missingMimeTypes.append(QLatin1String("application/x-shellscript"));
    }
    if (!KMimeType::mimeType(QLatin1String("application/x-executable"))) {
        missingMimeTypes.append(QLatin1String("application/x-executable"));
    }
    if (!KMimeType::mimeType(QLatin1String("application/x-desktop"))) {
        missingMimeTypes.append(QLatin1String("application/x-desktop"));
    }

    if (!missingMimeTypes.isEmpty()) {
        kError() << "could not find mime types" << missingMimeTypes;
    }
}

KMimeType::Ptr KMimeTypeRepository::defaultMimeTypePtr()
{
    if (!m_defaultMimeType) {
        // Try to find the default type
        KMimeType::Ptr mime = findMimeTypeByName(KMimeType::defaultMimeType());
        if (mime) {
            m_defaultMimeType = mime;
        } else {
            kError() << "could not find default mime type" << KMimeType::defaultMimeType();
        }
    }
    return m_defaultMimeType;

}

bool KMimeTypeRepository::useFavIcons()
{
    // this method will be called quite often, so better not read the config
    // again and again.
    if (!m_useFavIconsChecked) {
        m_useFavIconsChecked = true;
        KConfigGroup cg(KGlobal::config(), "HTML Settings");
        m_useFavIcons = cg.readEntry("EnableFavicon", true);
    }
    return m_useFavIcons;
}

int KMimeTypeRepository::sharedMimeInfoVersion()
{
    if (m_sharedMimeInfoVersion == 0) {
        m_sharedMimeInfoVersion = mimeDataBaseVersion();
    }
    return m_sharedMimeInfoVersion;
}
