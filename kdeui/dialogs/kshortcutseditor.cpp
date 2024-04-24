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

#include "kshortcutseditor.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QTreeWidget>

#include "kaction.h"
#include "kactioncollection.h"
#include "kkeysequencewidget.h"
#include "kaboutdata.h"
#include "kconfiggroup.h"
#include "kglobal.h"
#include "klocale.h"
#include "kdebug.h"

class KShortcutsEditorPrivate
{
public:
    KShortcutsEditorPrivate();

    void init(KShortcutsEditor *parent, const KShortcutsEditor::ActionTypes actionType,
              const KShortcutsEditor::LetterShortcuts allowLetterShortcuts);

    KShortcutsEditor* parent;
    KShortcutsEditor::ActionTypes actionTypes;
    bool allowLetterShortcuts;
    QHBoxLayout* layout;
    QTreeWidget* treewidget;
    QList<KActionCollection*> actionCollections;
};

KShortcutsEditorPrivate::KShortcutsEditorPrivate()
    : layout(nullptr),
    treewidget(nullptr)
{
}

void KShortcutsEditorPrivate::init(KShortcutsEditor *_parent,
                                   const KShortcutsEditor::ActionTypes actionType,
                                   const KShortcutsEditor::LetterShortcuts _allowLetterShortcuts)
{
    parent = _parent;
    actionTypes = actionType;
    allowLetterShortcuts = (_allowLetterShortcuts == KShortcutsEditor::LetterShortcutsAllowed);

    layout = new QHBoxLayout(parent);
    parent->setLayout(layout);

    treewidget = new QTreeWidget(parent);
    treewidget->setColumnCount(3);
    QStringList treeheaders = QStringList()
        << i18n("Collection")
        << i18n("Local")
        << i18n("Global");
    treewidget->setHeaderLabels(treeheaders);
    treewidget->setRootIsDecorated(true);
    QHeaderView* treeheader = treewidget->header();
    treeheader->setMovable(false);
    treeheader->setStretchLastSection(false);
    treeheader->setResizeMode(0, QHeaderView::Stretch);
    treeheader->setResizeMode(1, QHeaderView::Stretch);
    treeheader->setResizeMode(2, QHeaderView::Stretch);
    treeheader->setSectionHidden(1, !(actionType & KShortcutsEditor::LocalAction));
    treeheader->setSectionHidden(2, !(actionType & KShortcutsEditor::GlobalAction));

    layout->addWidget(treewidget);
}

KShortcutsEditor::KShortcutsEditor(KActionCollection *collection, QWidget *parent,
                                   ActionTypes actionType, LetterShortcuts allowLetterShortcuts)
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->init(this, actionType, allowLetterShortcuts);
    addCollection(collection);
}

KShortcutsEditor::KShortcutsEditor(QWidget *parent, ActionTypes actionType,
                                   LetterShortcuts allowLetterShortcuts)
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->init(this, actionType, allowLetterShortcuts);
}

KShortcutsEditor::~KShortcutsEditor()
{
    delete d;
}

bool KShortcutsEditor::isModified() const
{
    // TODO: implement
    return false;
}

void KShortcutsEditor::clearCollections()
{
    d->actionCollections.clear();
    d->treewidget->clear();
}

void KShortcutsEditor::addCollection(KActionCollection *collection, const QString &title)
{
    if (collection->isEmpty()) {
        return;
    }
    d->actionCollections.append(collection);

    QString text = title;
    if (text.isEmpty()) {
        const KAboutData* aboutdata = collection->componentData().aboutData();
        if (aboutdata) {
            text = aboutdata->programName();
        }
    }
    if (text.isEmpty()) {
        text = collection->objectName();
    }
    if (text.isEmpty()) {
        text = QString::number(quintptr(collection));
    }

    QTreeWidgetItem* topitem = nullptr;
    for (int i = 0; i < d->treewidget->topLevelItemCount(); i++) {
        QTreeWidgetItem* treeitem = d->treewidget->topLevelItem(i);
        if (treeitem->text(0) == text) {
            topitem = treeitem;
            break;
        }
    }
    if (!topitem) {
        topitem = new QTreeWidgetItem();
        topitem->setText(0, text);
    }
    int rowcounter = 0;
    foreach (QAction *action, collection->actions()) {
        QTreeWidgetItem* actionitem = new QTreeWidgetItem(topitem);
        actionitem->setIcon(0, action->icon());
        actionitem->setText(0, action->iconText());
        if (d->actionTypes & KShortcutsEditor::LocalAction) {
            KKeySequenceWidget* localkswidget = new KKeySequenceWidget(d->treewidget);
            localkswidget->setModifierlessAllowed(d->allowLetterShortcuts);
            localkswidget->setCheckForConflictsAgainst(
                KKeySequenceWidget::LocalShortcuts | KKeySequenceWidget::StandardShortcuts
            );
            localkswidget->setCheckActionCollections(d->actionCollections);
            localkswidget->setKeySequence(action->shortcut());
            d->treewidget->setItemWidget(actionitem, 1, localkswidget);
        }
        if (d->actionTypes & KShortcutsEditor::GlobalAction) {
            KAction* kaction = qobject_cast<KAction*>(action);
            if (kaction) {
                KKeySequenceWidget* globalkswidget = new KKeySequenceWidget(d->treewidget);
                globalkswidget->setModifierlessAllowed(d->allowLetterShortcuts);
                globalkswidget->setCheckForConflictsAgainst(
                    KKeySequenceWidget::LocalShortcuts | KKeySequenceWidget::GlobalShortcuts
                    | KKeySequenceWidget::StandardShortcuts
                );
                globalkswidget->setCheckActionCollections(d->actionCollections);
                globalkswidget->setKeySequence(kaction->globalShortcut());
                d->treewidget->setItemWidget(actionitem, 2, globalkswidget);
            } else {
                kWarning() << "action is not KAction" << action;
            }
        }
        rowcounter++;
    }
    d->treewidget->addTopLevelItem(topitem);
    topitem->setExpanded(true);
}

void KShortcutsEditor::importConfiguration(KConfigBase *config)
{
    if (!config) {
        config = KGlobal::config().data();
    }

    if (d->actionTypes & KShortcutsEditor::LocalAction) {
        KConfigGroup group(config, "Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->readSettings(&group);
        }
    }
    if (d->actionTypes & KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Global Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->importGlobalShortcuts(&group);
        }
    }
}

void KShortcutsEditor::exportConfiguration(KConfigBase *config) const
{
    if (!config) {
        config = KGlobal::config().data();
    }

    if (d->actionTypes & KShortcutsEditor::LocalAction) {
        KConfigGroup group(config, "Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->writeSettings(&group, true);
        }
    }
    if (d->actionTypes & KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Global Shortcuts");
        foreach (KActionCollection* collection, d->actionCollections) {
            collection->exportGlobalShortcuts(&group, true);
        }
    }
}

void KShortcutsEditor::allDefault()
{
    // TODO: implement
}

#include "moc_kshortcutseditor.cpp"
