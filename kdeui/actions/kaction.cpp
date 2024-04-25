/* This file is part of the KDE libraries
    Copyright (C) 1999 Reginald Stadlbauer <reggie@kde.org>
              (C) 1999 Simon Hausmann <hausmann@kde.org>
              (C) 2000 Nicolas Hadacek <haadcek@kde.org>
              (C) 2000 Kurt Granroth <granroth@kde.org>
              (C) 2000 Michael Koch <koch@kde.org>
              (C) 2001 Holger Freyther <freyther@kde.org>
              (C) 2002 Ellis Whitehead <ellis@kde.org>
              (C) 2002 Joseph Wenninger <jowenn@kde.org>
              (C) 2005-2006 Hamish Rodda <rodda@kde.org>

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

#include "kaction.h"
#include "kaction_p.h"
#include "kglobalaccel_p.h"
#include "klocale.h"
#include "kmessagebox.h"
#include "kdebug.h"
#include "kicon.h"

#include <QtGui/QApplication>
#include <QtGui/QHBoxLayout>
#include <QtGui/qevent.h>
#include <QtGui/QToolBar>

//---------------------------------------------------------------------
// KActionPrivate
//---------------------------------------------------------------------

KActionPrivate::KActionPrivate(KAction *q_ptr)
    : componentData(KGlobal::mainComponent()),
    globalShortcutEnabled(false),
    q(q_ptr)
{
    QObject::connect(q, SIGNAL(triggered(bool)), q, SLOT(slotTriggered()));
    q->setProperty("isShortcutConfigurable", true);
}

void KActionPrivate::slotTriggered()
{
    emit q->triggered(QApplication::mouseButtons(), QApplication::keyboardModifiers());
}

bool KAction::event(QEvent *event)
{
    if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *se = static_cast<QShortcutEvent*>(event);
        if(se->isAmbiguous()) {
            KMessageBox::information(
                NULL,  // No widget to be seen around here
                i18n(
                    "The key sequence '%1' is ambiguous. Use 'Configure Shortcuts'\n"
                    "from the 'Settings' menu to solve the ambiguity.\n"
                    "No action will be triggered.", se->key().toString(QKeySequence::NativeText)
                ),
                i18n("Ambiguous shortcut detected")
            );
            se->accept();
            return true;
        }
    }
    return QAction::event(event);
}


//---------------------------------------------------------------------
// KAction
//---------------------------------------------------------------------
KAction::KAction(QObject *parent)
    : QWidgetAction(parent),
    d(new KActionPrivate(this))
{
}

KAction::KAction(const QString &text, QObject *parent)
    : QWidgetAction(parent),
    d(new KActionPrivate(this))
{
    setText(text);
}

KAction::KAction(const KIcon &icon, const QString &text, QObject *parent)
    : QWidgetAction(parent),
    d(new KActionPrivate(this))
{
    setIcon(icon);
    setText(text);
}

KAction::~KAction()
{
    if (d->globalShortcutEnabled) {
        // - remove the action from KGlobalAccel
        d->globalShortcutEnabled = false;
        KGlobalAccel::self()->d->remove(this);
    }
    delete d;
}

bool KAction::isShortcutConfigurable() const
{
    return property("isShortcutConfigurable").toBool();
}

void KAction::setShortcutConfigurable(bool b)
{
    setProperty("isShortcutConfigurable", b);
}

QKeySequence KAction::shortcut(ShortcutTypes type) const
{
    Q_ASSERT(type);
    if (type == DefaultShortcut) {
        return d->defaultShortcut;
    }
    return QAction::shortcut();
}

void KAction::setShortcut(const QKeySequence &keySeq, ShortcutTypes type)
{
    Q_ASSERT(type);
    if (type & KAction::DefaultShortcut) {
        d->defaultShortcut = keySeq;
    }
    if (type & KAction::ActiveShortcut) {
        QAction::setShortcut(keySeq);
    }
}

const QKeySequence& KAction::globalShortcut(ShortcutTypes type) const
{
    Q_ASSERT(type);
    if (type == KAction::DefaultShortcut) {
        return d->defaultGlobalShortcut;
    }
    return d->globalShortcut;
}

void KAction::setGlobalShortcut(const QKeySequence &shortcut, ShortcutTypes type)
{
    Q_ASSERT(type);
    if (!d->globalShortcutEnabled) {
        if (objectName().isEmpty() || objectName().startsWith(QLatin1String("unnamed-"))) {
            kWarning(129) << "Attempt to set global shortcut for action without objectName()."
                             " Read the setGlobalShortcut() documentation.";
            return;
        }
        d->globalShortcutEnabled = true;
    }

    if ((type & KAction::DefaultShortcut) && d->defaultGlobalShortcut != shortcut) {
        d->defaultGlobalShortcut = shortcut;
    }

    if ((type & KAction::ActiveShortcut) && d->globalShortcut != shortcut) {
        d->globalShortcut = shortcut;
        if (KGlobalAccel::self()->d->remove(this)) {
            KGlobalAccel::self()->d->doRegister(this);
        }
        emit globalShortcutChanged(d->globalShortcut);
    }
}


bool KAction::isGlobalShortcutEnabled() const
{
    return d->globalShortcutEnabled;
}

void KAction::forgetGlobalShortcut()
{
    d->globalShortcut = QKeySequence();
    d->defaultGlobalShortcut = QKeySequence();
    if (d->globalShortcutEnabled) {
        d->globalShortcutEnabled = false;
        KGlobalAccel::self()->d->remove(this);
        emit globalShortcutChanged(d->globalShortcut);
    }
}

void KAction::setHelpText(const QString &text)
{
    setStatusTip(text);
    setToolTip(text);
    if (whatsThis().isEmpty()) {
        setWhatsThis(text);
    }
}

#include "moc_kaction.cpp"
