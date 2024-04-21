/* This file is part of the KDE libraries
    Copyright (C) 2001,2002 Ellis Whitehead <ellis@kde.org>
    Copyright (C) 2006 Hamish Rodda <rodda@kde.org>
    Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>

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

#ifndef KGLOBALACCEL_H
#define KGLOBALACCEL_H

#include <QWidget>

#include "kdeui_export.h"
#include "kaction.h"
#include "kglobal.h"

struct KGlobalShortcutInfo
{
    QString componentFriendlyName;
    QString friendlyName;
    QString contextFriendlyName;
};

/**
 * @short Configurable global shortcut support
 *
 * KGlobalAccel allows you to have global accelerators that are independent of
 * the focused window.  Unlike regular shortcuts, the application's window does not need focus
 * for them to be activated.
 *
 * @see KKeyChooser
 * @see KKeyDialog
 */
class KDEUI_EXPORT KGlobalAccel : public QObject
{
    Q_OBJECT

public:
    /// Creates a new KGlobalAccel object
    KGlobalAccel();

    /// Destructor
    ~KGlobalAccel();

    /**
     * Returns (and creates if necessary) the singleton instance
     */
    static KGlobalAccel *self();

    /**
     * Returns a list of global shortcuts registered for the shortcut @seq.
     *
     * If the list contains more that one entry it means the component
     * that registered the shortcuts uses global shortcut contexts. All
     * returned shortcuts belong to the same component.
     *
     * @since 4.2
     */
    QList<KGlobalShortcutInfo> getGlobalShortcutsByKey(const QKeySequence &seq);

    /**
     * Check if the shortcut @seq is available for the @p component. The
     * component is only of interest if the current application uses global shortcut
     * contexts. In that case a global shortcut by @p component in an inactive
     * global shortcut contexts does not block the @p seq for us.
     *
     * @since 4.2
     */
    bool isGlobalShortcutAvailable(const QKeySequence &seq,
                                   const QString &component = QString());

    /**
     * Take away the given shortcut from the named action it belongs to.
     * This applies to all actions with global shortcuts in any KDE application.
     *
     * @see promptStealShortcutSystemwide()
     */
    void stealShortcutSystemwide(const QKeySequence &seq);

    /**
     * Show a messagebox to inform the user that a global shorcut is already occupied,
     * and ask to take it away from its current action(s). This is GUI only, so nothing will
     * be actually changed.
     *
     * @see stealShortcutSystemwide()
     *
     * @since 4.2
     */
    static bool promptStealShortcutSystemwide(QWidget *parent,
                                              const QList<KGlobalShortcutInfo> &shortcuts,
                                              const QKeySequence &seq);

private:
    friend class KAction;
    class KGlobalAccelPrivate *const d;
};

#endif // KGLOBALACCEL_H
