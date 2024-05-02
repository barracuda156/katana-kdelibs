// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>

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

#ifndef KIO_CONNECTION_P_H
#define KIO_CONNECTION_P_H

#include <QQueue>
#include <QLocalServer>
#include <QLocalSocket>

namespace KIO {

    struct Task {
        int cmd;
        QByteArray data;
    };

    class SocketConnectionBackend: public QObject
    {
        Q_OBJECT
    public:
        QString address;
        QString errorString;
        enum { Idle, Listening, Connected } state;

    private:
        enum { HeaderSize = 10, StandardBufferSize = 32*1024 };

        QLocalSocket *socket;
        QLocalServer *localServer;
        long len;
        int cmd;
        bool signalEmitted;

    public:
        explicit SocketConnectionBackend(QObject *parent = nullptr);

        void setSuspended(bool enable);
        bool connectToRemote(const QString &address);
        bool listenForRemote();
        bool waitForIncomingTask(int ms);
        bool sendCommand(const Task &task);
        SocketConnectionBackend* nextPendingConnection();

    public slots:
        void socketReadyRead();
        void socketDisconnected();

    Q_SIGNALS:
        void disconnected();
        void commandReceived(const Task &task);
        void newConnection();
    };

    /**
     * @private
     *
     * This class provides a simple means for IPC between two applications
     * via a pipe.
     * It handles a queue of commands to be sent which makes it possible to
     * queue data before an actual connection has been established.
     */
    class Connection : public QObject
    {
        Q_OBJECT
    public:
        /**
         * Creates a new connection.
         * @see connectToRemote, listenForRemote
        */
        explicit Connection(QObject *parent = 0);
        virtual ~Connection();

        /**
         * Connects to the remote address.
         */
        void connectToRemote(const QString &address);

        /// Closes the connection.
        void close();

        QString errorString() const;

        bool isConnected() const;

        /**
         * Sends/queues the given command to be sent.
         * @param cmd the command to set
         * @param arr the bytes to send
         * @return true if successful, false otherwise
         */
        bool send(int cmd, const QByteArray &arr = QByteArray());

        /**
         * Sends the given command immediately.
         * @param _cmd the command to set
         * @param data the bytes to send
         * @return true if successful, false otherwise
         */
        bool sendnow(int cmd, const QByteArray &data);

        /**
         * Returns true if there are packets to be read immediately,
         * false if waitForIncomingTask must be called before more data
         * is available.
         */
        bool hasTaskAvailable() const;

        /**
         * Waits for one more command to be handled and ready.
         *
         * @param ms   the time to wait in milliseconds
         * @returns true if one command can be read, false if we timed out
         */
        bool waitForIncomingTask(int ms);

        /**
         * Receive data.
         *
         * @param _cmd the received command will be written here
         * @param data the received data will be written here
         * @return >=0 indicates the received data size upon success
         *         -1  indicates error
         */
        int read(int *cmd, QByteArray &data);

        /**
         * Don't handle incoming data until resumed.
         */
        void suspend();

        /**
         * Resume handling of incoming data.
         */
        void resume();

        /**
         * Returns status of connection.
        * @return true if suspended, false otherwise
         */
        bool suspended() const;

    Q_SIGNALS:
        void readyRead();

    private Q_SLOTS:
        void dequeue();
        void commandReceived(const Task &task);
        void disconnected();

    private:
        Q_DISABLE_COPY(Connection)

        friend class ConnectionServer;

        void setBackend(SocketConnectionBackend *b);

        QQueue<Task> m_outgoingTasks;
        QQueue<Task> m_incomingTasks;
        SocketConnectionBackend* m_backend;
        bool m_suspended;
    };

    /**
     * @private
     *
     * This class provides a way to obtaining KIO::Connection connections.
     */
    class ConnectionServer : public QObject
    {
        Q_OBJECT
    public:
        ConnectionServer(QObject *parent = nullptr);

        /**
         * Sets this connection to listen mode. Use address() to obtain the
         * address this is listening on.
         */
        void listenForRemote();
        bool isListening() const;
        /// Closes the connection.
        void close();

        /**
         * Returns the address for this connection if it is listening, an empty
         * string if not.
         */
        QString address() const;

        Connection *nextPendingConnection();
        void setNextPendingConnection(Connection *conn);

    Q_SIGNALS:
        void newConnection();

    private:
        Q_DISABLE_COPY(ConnectionServer)

        SocketConnectionBackend *m_backend;
    };

}

Q_DECLARE_METATYPE(KIO::Task)

#endif
