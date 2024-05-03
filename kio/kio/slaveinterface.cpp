/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>
   Copyright (c) 2000 Waldo Bastian <bastian@kde.org>
   Copyright (c) 2000 Stephan Kulow <coolo@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "slaveinterface_p.h"
#include "usernotificationhandler_p.h"
#include "slavebase.h"
#include "connection_p.h"
#include "job_p.h"
#include "klocale.h"
#include "kprotocolinfo.h"
#include "kstandarddirs.h"
#include "kdebug.h"

#include <QProcess>
#include <QDir>

#include <unistd.h>
#include <signal.h>

using namespace KIO;

Q_GLOBAL_STATIC(UserNotificationHandler, globalUserNotificationHandler)

SlaveInterface::SlaveInterface(const QString &protocol, QObject *parent)
    : QObject(parent),
    m_offset(0),
    m_protocol(protocol),
    m_processedsize(0),
    m_totalsize(0),
    m_lasttime(0),
    m_connection(nullptr),
    m_slaveconnserver(nullptr),
    m_pid(0),
    m_dead(false),
    m_refcount(1)
{
    connect(&m_speedtimer, SIGNAL(timeout()), SLOT(calcSpeed()));
    m_slaveconnserver = new KIO::ConnectionServer(this);
    m_connection = new KIO::Connection(this);
    connect(m_slaveconnserver, SIGNAL(newConnection()), SLOT(accept()));

    m_slaveconnserver->listenForRemote();
    if (!m_slaveconnserver->isListening()) {
        kWarning() << "Connection server not listening, could not connect";
    }
}

SlaveInterface::~SlaveInterface()
{
    // Note: no kDebug() here (scheduler is deleted very late)
    delete m_slaveconnserver;
    delete m_connection;
}

QString SlaveInterface::protocol() const
{
    return m_protocol;
}

QString SlaveInterface::host() const
{
    return m_host;
}

void SlaveInterface::setHost(const QString &host)
{
    m_host = host;
}

void SlaveInterface::ref()
{
    m_refcount++;
}

void SlaveInterface::deref()
{
    m_refcount--;
    if (!m_refcount) {
        m_connection->disconnect(this);
        this->disconnect();
        deleteLater();
    }
}

void SlaveInterface::setPID(const pid_t pid)
{
    m_pid = pid;
}

pid_t SlaveInterface::pid() const
{
    return m_pid;
}

bool SlaveInterface::isAlive() const
{
    return !m_dead;
}

void SlaveInterface::suspend()
{
    m_connection->suspend();
}

void SlaveInterface::resume()
{
    m_connection->resume();
}

bool SlaveInterface::suspended() const
{
    return m_connection->suspended();
}

void SlaveInterface::send(int cmd, const QByteArray &arr)
{
    m_connection->send(cmd, arr);
}

void SlaveInterface::kill()
{
    m_dead = true; // OO can be such simple.
    kDebug(7002) << "killing slave pid" << m_pid
                 << "(" << m_protocol << m_host << ")";
    if (m_pid) {
       ::kill(m_pid, SIGTERM);
       m_pid = 0;
    }
}

void SlaveInterface::setConfig(const MetaData &config)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << config;
    m_connection->send(CMD_CONFIG, data);
}

SlaveInterface* SlaveInterface::createSlave(const QString &protocol, const KUrl &url, int &error, QString &error_text)
{
    kDebug(7002) << "createSlave" << protocol << "for" << url;
    SlaveInterface *slave = new SlaveInterface(protocol);
    const QString slaveaddress = slave->m_slaveconnserver->address();

    const QString slavename = KProtocolInfo::exec(protocol);
    if (slavename.isEmpty()) {
        error_text = i18n("Unknown protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
        delete slave;
        return 0;
    }
    const QString slaveexe = KStandardDirs::locate("libexec", slavename);
    if (slaveexe.isEmpty()) {
        error_text = i18n("Can not find io-slave for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
        delete slave;
        return 0;
    }

    kDebug() << "kioslave" << ", " << slaveexe << ", " << protocol << ", " << slaveaddress;

    const QStringList slaveargs = QStringList() << slaveexe << slaveaddress;
    Q_PID slavepid = 0;
    const bool result = QProcess::startDetached(
        KStandardDirs::findExe("kioslave"),
        slaveargs,
        QDir::currentPath(),
        &slavepid
    );
    if (!result || !slavepid) {
        error_text = i18n("Can not start io-slave for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
        delete slave;
        return 0;
    }
    slave->setPID(slavepid);

    return slave;
}

bool SlaveInterface::dispatch()
{
    Q_ASSERT(m_connection);
    int cmd = 0;
    QByteArray data;
    int ret = m_connection->read(&cmd, data);
    if (ret == -1) {
        return false;
    }
    return dispatch(cmd, data);
}

void SlaveInterface::calcSpeed()
{
    unsigned long lspeed = 0;
    if (m_lasttime > 0 && m_processedsize > 0) {
        lspeed = (m_processedsize - m_lasttime);
        if (lspeed < 0) {
            kWarning() << "speed is negative" << m_lasttime << m_processedsize << lspeed;
            lspeed = 0;
        }
    }
    emit speed(lspeed);
    if (m_processedsize > 0) {
        m_lasttime = m_processedsize;
    } else {
        m_lasttime = 0;
    }
}

bool SlaveInterface::dispatch(int cmd, const QByteArray &rawdata)
{
    // kDebug(7007) << "dispatch " << cmd;

    switch(cmd) {
        case MSG_DATA: {
            emit data(rawdata);
            break;
        }
        case MSG_DATA_REQ: {
            emit dataReq();
            break;
        }
        case MSG_FINISHED: {
            // kDebug(7007) << "Finished [this = " << this << "]";
            m_offset = 0;
            m_speedtimer.stop();
            emit finished();
            break;
        }
        case MSG_STAT_ENTRY: {
            QDataStream stream(rawdata);
            UDSEntry entry;
            stream >> entry;
            emit statEntry(entry);
            break;
        }
        case MSG_LIST_ENTRIES: {
            QDataStream stream(rawdata);
            quint32 count = 0;
            stream >> count;
            UDSEntryList list;
            list.reserve(count);
            UDSEntry entry;
            for (uint i = 0; i < count; i++) {
                stream >> entry;
                list.append(entry);
            }
            emit listEntries(list);
            break;
        }
        case MSG_RESUME: {
            // From the put job
            QDataStream stream(rawdata);
            stream >> m_offset;
            emit canResume(m_offset);
            break;
        }
        case MSG_CANRESUME: {
            // From the get job
            emit canResume(0); // the arg doesn't matter
            break;
        }
        case MSG_ERROR: {
            QDataStream stream(rawdata);
            qint32 i = 0;
            QString str;
            stream >> i >> str;
            kDebug(7007) << "error " << i << " " << str;
            emit error(i, str);
            break;
        }
        case INF_TOTAL_SIZE: {
            QDataStream stream(rawdata);
            KIO::filesize_t totalsize;
            stream >> totalsize;
            if (totalsize != m_totalsize) {
                // start again with speed calculation
                m_totalsize = totalsize;
                m_lasttime = 0;
            }
            emit totalSize(totalsize);
            break;
        }
        case INF_PROCESSED_SIZE: {
            QDataStream stream(rawdata);
            stream >> m_processedsize;
            if (!m_speedtimer.isActive()) {
                m_speedtimer.start(1000);
            }
            emit processedSize(m_processedsize);
            break;
        }
        case INF_REDIRECTION: {
            QDataStream stream(rawdata);
            KUrl url;
            stream >> url;
            emit redirection(url);
            break;
        }
        case INF_WARNING: {
            QDataStream stream(rawdata);
            QString str;
            stream >> str;
            emit warning(str);
            break;
        }
        case INF_MESSAGEBOX: {
            kDebug(7007) << "needs a msg box";
            QDataStream stream(rawdata);
            QString text, caption, buttonYes, buttonNo, dontAskAgainName;
            qint32 type = 0;
            stream >> type >> text >> caption >> buttonYes >> buttonNo >> dontAskAgainName;
            messageBox(type, text, caption, buttonYes, buttonNo, dontAskAgainName);
            break;
        }
        case INF_INFOMESSAGE: {
            QDataStream stream(rawdata);
            QString msg;
            stream >> msg;
            emit infoMessage(msg);
            break;
        }
        case INF_META_DATA: {
            QDataStream stream(rawdata);
            MetaData m;
            stream >> m;
            emit metaData(m);
            break;
        }
        default: {
            kWarning(7007) << "Slave sends unknown command (" << cmd << "), dropping slave";
            return false;
        }
    }
    return true;
}

void SlaveInterface::setOffset(const KIO::filesize_t o)
{
    m_offset = o;
}

KIO::filesize_t SlaveInterface::offset() const
{
    return m_offset;
}

void SlaveInterface::sendResumeAnswer(bool resume)
{
    kDebug(7007) << "ok for resuming:" << resume;
    m_connection->sendnow(resume ? CMD_RESUMEANSWER : CMD_NONE, QByteArray());
}

void SlaveInterface::sendMessageBoxAnswer(int result)
{
    if (!m_connection) {
        return;
    }

    if (m_connection->suspended()) {
        m_connection->resume();
    }
    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << result;
    m_connection->sendnow(CMD_MESSAGEBOXANSWER, packedArgs);
    kDebug(7007) << "message box answer" << result;
}

void SlaveInterface::messageBox(int type, const QString &text, const QString &caption,
                                const QString &buttonYes, const QString &buttonNo, const QString &dontAskAgainName)
{
    if (m_connection) {
        m_connection->suspend();
    }

    QHash<UserNotificationHandler::MessageBoxDataType, QString> data;
    data.insert(UserNotificationHandler::MSG_TEXT, text);
    data.insert(UserNotificationHandler::MSG_CAPTION, caption);
    data.insert(UserNotificationHandler::MSG_YES_BUTTON_TEXT, buttonYes);
    data.insert(UserNotificationHandler::MSG_NO_BUTTON_TEXT, buttonNo);
    data.insert(UserNotificationHandler::MSG_DONT_ASK_AGAIN, dontAskAgainName);

    // SMELL: the braindead way to support button icons
    // TODO: Fix this in KIO::SlaveBase.
    if (buttonYes == i18n("&Details")) {
        data.insert(UserNotificationHandler::MSG_YES_BUTTON_ICON, QLatin1String("help-about"));
    }

    if (buttonNo == i18n("Co&ntinue")) {
        data.insert(UserNotificationHandler::MSG_NO_BUTTON_ICON, QLatin1String("arrow-right"));
    }

    globalUserNotificationHandler()->requestMessageBox(this, type, data);
}

void SlaveInterface::accept()
{
    m_slaveconnserver->setNextPendingConnection(m_connection);
    m_slaveconnserver->deleteLater();
    m_slaveconnserver = nullptr;

    connect(m_connection, SIGNAL(readyRead()), SLOT(gotInput()));
}

void SlaveInterface::gotInput()
{
    if (m_dead) {
        // already dead? then slaveDied was emitted
        return;
    }
    ref();
    if (!dispatch()) {
        m_connection->close();
        m_dead = true;
        QString arg = m_protocol;
        if (!m_host.isEmpty()) {
            arg += QString::fromLatin1("://") + m_host;
        }
        kDebug(7002) << "slave died pid = " << m_pid << arg;
        // Tell the job about the problem.
        emit error(ERR_SLAVE_DIED, arg);
        // Tell the scheduler about the problem.
        emit slaveDied(this);
    }
    deref();
    // Here we might be dead!!
}

#include "moc_slaveinterface_p.cpp"
