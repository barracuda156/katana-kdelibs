/*
    This file is part of the KDE libraries
     Copyright
     (C) 2000 Reginald Stadlbauer (reggie@kde.org)
     (C) 1997 Stephan Kulow (coolo@kde.org)
     (C) 1997-2000 Sven Radej (radej@kde.org)
     (C) 1997-2000 Matthias Ettrich (ettrich@kde.org)
     (C) 1999 Chris Schlaeger (cs@kde.org)
     (C) 2002 Joseph Wenninger (jowenn@kde.org)
     (C) 2005-2006 Hamish Rodda (rodda@kde.org)

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

#ifndef KMAINWINDOW_P_H
#define KMAINWINDOW_P_H

#include <kconfiggroup.h>
#include <QPointer>
#include <QObject>
#include <QTimer>

#define K_D(Class) Class##Private * const d = k_func()

class KHelpMenu;

/**
 * Listens to resize events from QDockWidgets. The KMainWindow settings are set as dirty as soon as
 * at least one resize event occurred. The listener is attached to the dock widgets installing
 * event filter inside KMainWindow::event().
 */
class DockResizeListener : public QObject
{
public:
    DockResizeListener(KMainWindow *win);

    bool eventFilter(QObject *watched, QEvent *event) final;

private:
    KMainWindow *m_win;
};


class KMainWindowPrivate
{
public:
    bool autoSaveSettings;
    bool settingsDirty;
    bool autoSaveWindowSize;
    bool care_about_geometry;
    bool sizeApplied;
    KConfigGroup autoSaveGroup;
    QTimer *settingsTimer;
    QTimer *sizeTimer;
    QRect defaultWindowSize;
    KHelpMenu *helpMenu;
    KMainWindow *q;
    QPointer<DockResizeListener> dockResizeListener;
    QString dbusName;
    bool letDirtySettings;

    // This slot will be called when the style KCM changes settings that need
    // to be set on the already running applications.
    void _k_slotStyleChanged();
    void _k_slotSaveAutoSaveSize();

    void init(KMainWindow *_q);
    void polish(KMainWindow *q);
    void setSettingsDirty(bool compressCalls = false);
    void setSizeDirty();
};

#endif

