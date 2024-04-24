/* This file is part of the KDE libraries Copyright (C) 1998 Mark Donohoe <donohoe@kde.org>
    Copyright (C) 1997 Nicolas Hadacek <hadacek@kde.org>
    Copyright (C) 1998 Matthias Ettrich <ettrich@kde.org>
    Copyright (C) 2001 Ellis Whitehead <ellis@kde.org>
    Copyright (C) 2006 Hamish Rodda <rodda@kde.org>
    Copyright (C) 2007 Roberto Raggi <roberto@kdevelop.org>
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

#include "kshortcutseditor.h"

#include <QHeaderView>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QTextDocument>
#include <QTextTable>
#include <QTextCursor>
#include <QTextFormat>
#include <QPrinter>
#include <QPrintDialog>

#include "kaction.h"
#include "kactioncollection.h"
#include "kactioncategory.h"
#include "kdebug.h"
#include "kdeprintdialog.h"
#include "kglobalaccel.h"
#include "kmessagebox.h"
#include "kaboutdata.h"
#include "kconfiggroup.h"

class KShortcutsEditorPrivate
{
public:
    KShortcutsEditor::ActionTypes actionTypes;
    QList<KActionCollection*> actionCollections;
};

KShortcutsEditor::KShortcutsEditor(KActionCollection *collection, QWidget *parent, ActionTypes actionType,
                                   LetterShortcuts allowLetterShortcuts )
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->actionTypes = actionType;
    addCollection(collection);
}

KShortcutsEditor::KShortcutsEditor(QWidget *parent, ActionTypes actionType, LetterShortcuts allowLetterShortcuts)
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->actionTypes = actionType;
}

KShortcutsEditor::~KShortcutsEditor()
{
    delete d;
}

bool KShortcutsEditor::isModified() const
{
    return false;
}

void KShortcutsEditor::clearCollections()
{
    d->actionCollections.clear();
}

void KShortcutsEditor::addCollection(KActionCollection *collection, const QString &title)
{
    // KXmlGui add action collections unconditionally. If some plugin doesn't
    // provide actions we don't want to create empty subgroups.
    if (collection->isEmpty()) {
        return;
    }


    d->actionCollections.append(collection);
}

void KShortcutsEditor::clearConfiguration()
{
}

void KShortcutsEditor::importConfiguration(KConfigBase *config)
{
}

void KShortcutsEditor::exportConfiguration(KConfigBase *config) const
{
    Q_ASSERT(config);
    if (!config) return;

    if (d->actionTypes & KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Global Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->exportGlobalShortcuts(&group, true);
        }
    }
    if (d->actionTypes & ~KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->writeSettings(&group, true);
        }
    }
}

void KShortcutsEditor::writeConfiguration(KConfigGroup *config) const
{
    foreach (KActionCollection* collection, d->actionCollections) {
        collection->writeSettings(config);
    }
}

void KShortcutsEditor::commit()
{
}

void KShortcutsEditor::save()
{
    writeConfiguration();
    commit();
}

void KShortcutsEditor::undoChanges()
{
}


void KShortcutsEditor::allDefault()
{
}

#include "moc_kshortcutseditor.cpp"
