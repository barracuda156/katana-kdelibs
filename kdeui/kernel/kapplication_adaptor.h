/*
    This file is part of the KDE libraries
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KAPPLICATION_ADAPTOR_H
#define KAPPLICATION_ADAPTOR_H

#include <QDBusAbstractAdaptor>

class KApplicationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KApplication")
public:
    KApplicationAdaptor(QObject *parent);

Q_SIGNALS:
    void sessionSaved();
    void sessionSaveCanceled();

public Q_SLOTS:
    QStringList restartCommand() const;
    void updateUserTimestamp(int time = 0);
    void saveSession();
    void reparseConfiguration();
    void quit();

private:
    Q_DISABLE_COPY(KApplicationAdaptor);
};

#endif // KAPPLICATION_ADAPTOR_H

