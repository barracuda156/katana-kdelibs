/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KIO_SLAVEINTERFACE_H
#define KIO_SLAVEINTERFACE_H

#include "kio/global.h"
#include "kio/udsentry.h"
#include "kio/connection_p.h"

#include <QTimer>
#include <QElapsedTimer>

#include <unistd.h>
#include <sys/types.h>

class KUrl;

namespace KIO {

enum SlaveMessage {
    /**
     * Identifiers for slave interface messages, used for communication between SlaveBase and
     * SlaveInterface. Must not clash with job commands! Also zero is reserved for invalid command.
     */
    SI_TOTAL_SIZE = 1,
    SI_PROCESSED_SIZE = 2,
    SI_REDIRECTION = 3,
    SI_WARNING = 4,
    SI_INFOMESSAGE = 5,
    SI_META_DATA = 6,
    SI_MESSAGEBOX = 7,
    SI_DATA = 8,
    SI_DATA_REQ = 9,
    SI_ERROR = 10,
    SI_FINISHED = 11,
    SI_STAT_ENTRY = 12,
    SI_LIST_ENTRIES = 13,
    SI_RESUME = 14,
    SI_CANRESUME = 15
};

/**
 * There are two classes that specifies the protocol between application
 * ( KIO::Job) and kioslave. SlaveInterface is the class to use on the application
 * end, SlaveBase is the one to use on the slave end.
 *
 * A call to foo() results in a call to slotFoo() on the other end.
 */
class SlaveInterface : public QObject
{
    Q_OBJECT
public:
    explicit SlaveInterface(const QString &protocol, QObject *parent = nullptr);
    ~SlaveInterface();

    void setPID(const pid_t pid);
    pid_t pid() const;

    /**
     * Force termination
     */
    void kill();

    /**
     * @return true if the slave survived the last mission.
     */
    bool isAlive() const;

    /**
     * Configure slave
     */
    void setConfig(const MetaData &config);

    /**
     * The protocol this slave handles.
     *
     * @return name of protocol handled by this slave, as seen by the user
     */
    QString protocol() const;

    /**
     * @return Host this slave is (was?) connected to
     */
    QString host() const;

    /**
     * Set host for url, includes everything but the path
     * @param host to connect to.
     */
    void setHost(const QString &host);

    /**
     * Creates a new slave.
     *
     * @param protocol the protocol
     * @param url is the url
     * @param error is the error code on failure and undefined else.
     * @param error_text is the error text on failure and undefined else.
     *
     * @return 0 on failure, or a pointer to a slave otherwise.
     */
    static SlaveInterface* createSlave(const QString &protocol, const KUrl &url, int &error, QString &error_text);

    // == communication with connected kioslave ==
    /**
     * Suspends the operation of the attached kioslave.
     */
    void suspend();

    /**
     * Resumes the operation of the attached kioslave.
     */
    void resume();

    /**
     * Tells whether the kioslave is suspended.
     * @return true if the kioslave is suspended.
     */
    bool suspended() const;

    /**
     * Sends the given command to the kioslave.
     * @param cmd command id
     * @param arr byte array containing data
     */
    void send(int cmd, const QByteArray &arr);
    // == end communication with connected kioslave ==

    /**
     * @return The time this slave has been idle.
     */
    qint64 idleTime() const;

    /**
     * Marks or unmarks this slave as idle.
     */
    void setIdle(const bool idle);

    void ref();
    void deref();

    // Send our answer to the MSG_RESUME (canResume) request
    // (to tell the "put" job whether to resume or not)
    void sendResumeAnswer(bool resume);

    /**
     * Sends our answer for the INF_MESSAGEBOX request.
     */
    void sendMessageBoxAnswer(int result);

    void setOffset(const KIO::filesize_t offset);
    KIO::filesize_t offset() const;

Q_SIGNALS:
    ///////////
    // Messages sent by the slave
    ///////////
    void data(const QByteArray &);
    void dataReq();
    void error(int , const QString &);
    void finished();
    void listEntries(const KIO::UDSEntryList &);
    void statEntry(const KIO::UDSEntry &);
    void canResume(KIO::filesize_t );

    ///////////
    // Info sent by the slave
    //////////
    void metaData(const KIO::MetaData &);
    void totalSize(KIO::filesize_t );
    void processedSize(KIO::filesize_t );
    void redirection(const KUrl &);
    void speed(unsigned long );
    void warning(const QString &);
    void infoMessage(const QString &);

    ///////////
    // Info sent for the scheduler
    //////////
    void slaveDied(KIO::SlaveInterface *slave);

protected:
    /////////////////
    // Dispatching
    ////////////////
    bool dispatch();
    bool dispatch(int cmd, const QByteArray &data);

    void messageBox(int type, const QString &text, const QString &caption,
                    const QString &buttonYes, const QString &buttonNo,
                    const QString &dontAskAgainName);

protected Q_SLOTS:
    void calcSpeed();
    void accept();
    void gotInput();

private:
    Q_DISABLE_COPY(SlaveInterface);

    KIO::filesize_t m_offset;
    QString m_protocol;
    QString m_host;

    KIO::filesize_t m_processedsize;
    KIO::filesize_t m_totalsize;
    KIO::filesize_t m_lasttime;
    QTimer m_speedtimer;

    KIO::Connection* m_connection;
    KIO::ConnectionServer* m_slaveconnserver;
    pid_t m_pid;
    bool m_dead;
    int m_refcount;
    QElapsedTimer m_idlesince;
};

}

#endif
