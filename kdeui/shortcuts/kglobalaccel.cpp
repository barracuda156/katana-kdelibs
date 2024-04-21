/* This file is part of the KDE libraries
    Copyright (C) 2001,2002 Ellis Whitehead <ellis@kde.org>
    Copyright (C) 2006 Hamish Rodda <rodda@kde.org>
    Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>
    Copyright (C) 2008 Michael Jansen <kde@michael-jansen.biz>

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

#include "kglobalaccel.h"
#include "kglobalaccel_p.h"

#include "kapplication.h"
#include "klocale.h"
#include "kaboutdata.h"
#include "kaction_p.h"
#include "kmessagebox.h"
#include "kkeyserver.h"
#include "kxerrorhandler.h"
#include "kdebug.h"

K_GLOBAL_STATIC(KGlobalAccel, kGlobalAccel)

struct KGlobalAccelStruct
{
    KAction* action;
    uint keyModX;
    int keyCodeX;
    
    bool operator==(const KGlobalAccelStruct &other) const
    { return (action == other.action && keyModX == other.keyModX && keyCodeX == other.keyCodeX); }
};

extern "C" {
    static int XGrabErrorHandler(Display *, XErrorEvent *e) {
        if (e->error_code != BadAccess) {
            kWarning() << "grabKey: got X error " << e->type << " instead of BadAccess";
        }
        return 1;
    }
}

static bool kGrabKey(const int keyQt, uint &keyModX, int &keyCodeX)
{
    if (keyQt == 0) {
        kDebug() << "null keyQt";
        return false;
    }

    Display* display = QX11Info::display();
    const Qt::HANDLE approotwindow = QX11Info::appRootWindow();
    if (!display || !approotwindow) {
        kWarning() << "null display or application root window";
        return false;
    }

    uint keySymX = 0;
    if (!KKeyServer::keyQtToModX(keyQt, &keyModX)) {
        kWarning() << "keyQt (0x" << QByteArray::number(keyQt, 16) << ") failed to resolve to x11 modifier";
        return false;
    }
    if (!KKeyServer::keyQtToSymX(keyQt, (int *)&keySymX) ) {
        kWarning() << "keyQt (0x" << QByteArray::number(keyQt, 16) << ") failed to resolve to x11 keycode";
        return false;
    }

    keyCodeX = XKeysymToKeycode(display, keySymX);
    if (!keyCodeX) {
        kWarning() << "keyQt (0x" << QByteArray::number(keyQt, 16) << ") was resolved to x11 keycode 0";
        return false;
    }

    KXErrorHandler handler(XGrabErrorHandler);
    XGrabKey(
        display, keyCodeX, keyModX & KKeyServer::accelModMaskX(),
        approotwindow, True, GrabModeAsync, GrabModeAsync
    );
    return !handler.error(true);
}

static bool kUngrabKey(const uint keyModX, const int keyCodeX)
{
    Display* display = QX11Info::display();
    const Qt::HANDLE approotwindow = QX11Info::appRootWindow();
    if (!display || !approotwindow) {
        kWarning() << "null display or application root window";
        return false;
    }
    KXErrorHandler handler(XGrabErrorHandler);
    XUngrabKey(display, keyCodeX, keyModX & KKeyServer::accelModMaskX(), approotwindow);
    return !handler.error(true);
}

class KGlobalAccelFilter : public QWidget
{
public:
    QList<KGlobalAccelStruct> shortcuts; 

protected:
    bool x11Event(XEvent *xevent) final;
};

bool KGlobalAccelFilter::x11Event(XEvent *xevent)
{
    if (xevent->type == KeyPress) {
        foreach (const KGlobalAccelStruct &shortcut, shortcuts) {
            if (xevent->xkey.state == shortcut.keyModX && xevent->xkey.keycode == shortcut.keyCodeX) {
                kDebug() << "triggering action" << shortcut.keyModX << shortcut.keyCodeX << shortcut.action;
                shortcut.action->trigger();
                return true;
            }
        }
    }
    return false;
}

KGlobalAccelPrivate::KGlobalAccelPrivate(KGlobalAccel *_q)
     : q(_q),
     filter(nullptr)
{
    if (kapp) {
        filter = new KGlobalAccelFilter();
        kapp->installX11EventFilter(filter);
        kDebug() << "KGlobalAccelFilter is installed";
    } else {
        kWarning() << "no KApplication instance, KGlobalAccel will not work";
    }
}

KGlobalAccelPrivate::~KGlobalAccelPrivate()
{
    if (filter) {
        if (kapp) {
            kDebug() << "removing KGlobalAccelFilter";
            kapp->removeX11EventFilter(filter);
        }

        QList<KGlobalAccelStruct> shortcuts = filter->shortcuts;
        kDebug() << "releasing shortcuts" << shortcuts.size();
        foreach (const KGlobalAccelStruct &shortcut, shortcuts) {
            remove(shortcut.action, KGlobalAccelPrivate::SetInactive);
        }
        delete filter;
    }

}

void KGlobalAccelPrivate::updateGlobalShortcut(KAction *action, uint flags)
{
    if (!remove(action, KGlobalAccelPrivate::SetInactive)) {
        return;
    }
    doRegister(action);
}

void KGlobalAccelPrivate::doRegister(KAction *action)
{
    foreach (const QKeySequence &keysequnece, action->globalShortcut().toList()) {
        uint keyModX = 0;
        int keyCodeX = 0;
        if (kGrabKey(keysequnece, keyModX, keyCodeX)) {
            KGlobalAccelStruct shortcut;
            shortcut.action = action;
            shortcut.keyModX = keyModX;
            shortcut.keyCodeX = keyCodeX;
            filter->shortcuts.append(shortcut);
            kDebug() << "grabbed shortcut" << shortcut.keyModX << shortcut.keyCodeX << shortcut.action;
            break;
        } else {
            kWarning() << "could not grab shortcut" << keysequnece << action;
        }
    }
}

bool KGlobalAccelPrivate::remove(KAction *action, Removal r)
{
    Q_UNUSED(r);
    foreach (const KGlobalAccelStruct &shortcut, filter->shortcuts) {
        if (shortcut.action == action) {
            if (kUngrabKey(shortcut.keyModX, shortcut.keyCodeX)) {
                kDebug() << "ungrabbed shortcut" << shortcut.keyModX << shortcut.keyCodeX << shortcut.action;
                filter->shortcuts.removeOne(shortcut);
                return true;
            }
            kWarning() << "could not ungrab shortcut" << shortcut.keyModX << shortcut.keyCodeX << shortcut.action;
            return false;
        }
    }
    return true;
}


KGlobalAccel::KGlobalAccel()
    : d(new KGlobalAccelPrivate(this))
{
}

KGlobalAccel::~KGlobalAccel()
{
    delete d;
}

KGlobalAccel* KGlobalAccel::self()
{
    return kGlobalAccel;
}

QList<KGlobalShortcutInfo> KGlobalAccel::getGlobalShortcutsByKey(const QKeySequence &seq)
{
    QList<KGlobalShortcutInfo> result;
    foreach (const KGlobalAccelStruct &shortcut, d->filter->shortcuts) {
        if (shortcut.action->globalShortcut().contains(seq)) {
            KGlobalShortcutInfo globalshortcutinfo;
            globalshortcutinfo.componentFriendlyName = shortcut.action->d->componentData.aboutData()->programName();
            globalshortcutinfo.friendlyName = KGlobal::locale()->removeAcceleratorMarker(shortcut.action->text());
            globalshortcutinfo.contextFriendlyName = shortcut.action->objectName();
            result.append(globalshortcutinfo);
        }
    }
    return result;
}

bool KGlobalAccel::isGlobalShortcutAvailable(const QKeySequence &seq, const QString &comp)
{
    foreach (const KGlobalAccelStruct &shortcut, d->filter->shortcuts) {
        if (shortcut.action->globalShortcut().conflictsWith(seq)) {
            return false;
        }
    }
    return true;
}

void KGlobalAccel::stealShortcutSystemwide(const QKeySequence &seq)
{
    foreach (const KGlobalAccelStruct &shortcut, d->filter->shortcuts) {
        if (shortcut.action->globalShortcut().conflictsWith(seq)) {
            d->remove(shortcut.action, KGlobalAccelPrivate::SetInactive);
            break;
        }
    }
}

bool KGlobalAccel::promptStealShortcutSystemwide(QWidget *parent,
                                                 const QList<KGlobalShortcutInfo> &shortcuts,
                                                 const QKeySequence &seq)
{
    if (shortcuts.isEmpty()) {
        // Usage error. Just say no
        return false;
    }

    QString component = shortcuts[0].componentFriendlyName;

    QString message;
    if (shortcuts.size() ==1) {
        message = i18n("The '%1' key combination is registered by application %2 for action %3:",
                seq.toString(),
                component,
                shortcuts[0].friendlyName);
    } else {
        QString actionList;
        Q_FOREACH(const KGlobalShortcutInfo &info, shortcuts) {
            actionList += i18n("In context '%1' for action '%2'\n",
                    info.contextFriendlyName,
                    info.friendlyName);
        }
        message = i18n("The '%1' key combination is registered by application %2.\n%3",
                           seq.toString(),
                           component,
                           actionList);
    }

    QString title = i18n("Conflict With Registered Global Shortcut");

    return KMessageBox::warningContinueCancel(
        parent, message, title, KGuiItem(i18n("Reassign"))
    ) == KMessageBox::Continue;
}

#include "moc_kglobalaccel.cpp"

