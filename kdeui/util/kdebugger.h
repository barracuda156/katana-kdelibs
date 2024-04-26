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

#ifndef KDEBUGGER_H
#define KDEBUGGER_H

#include <kdeui_export.h>
#include <kdialog.h>

class KDebuggerPrivate;

/*!
    Debugging class

    @since 4.24
    @warning the API is subject to change
*/
class KDEUI_EXPORT KDebugger : public KDialog
{
    Q_OBJECT

public:
    KDebugger(QWidget *parent = nullptr);
    ~KDebugger();

private:
    Q_DISABLE_COPY(KDebugger);
    KDebuggerPrivate *const d;
};

#endif // KDEBUGGER_H
