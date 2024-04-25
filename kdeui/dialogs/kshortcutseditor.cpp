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
#include "kaction_p.h"
#include "kiconloader.h"
#include "kactioncollection.h"
#include "kkeysequencewidget.h"
#include "kaboutdata.h"
#include "kconfiggroup.h"
#include "kglobal.h"
#include "klocale.h"
#include "kdebug.h"

Q_DECLARE_METATYPE(QAction*)

static QTreeWidgetItem* kMakeActionItem(QTreeWidgetItem *parent, QAction *action)
{
    QTreeWidgetItem* actionitem = new QTreeWidgetItem(parent);
    actionitem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    actionitem->setIcon(0, action->icon());
    actionitem->setText(0, action->iconText());
    actionitem->setToolTip(0, action->toolTip());
    actionitem->setStatusTip(0, action->statusTip());
    return actionitem;
}

class KShortcutsEditorPrivate
{
public:
    KShortcutsEditorPrivate();

    void init(KShortcutsEditor *parent, const KShortcutsEditor::ActionTypes actionTypes,
              const KShortcutsEditor::LetterShortcuts letterShortcuts);

    void _k_slotKeySequenceChanged();

    KShortcutsEditor* parent;
    KShortcutsEditor::ActionTypes actiontypes;
    bool allowlettershortcuts;
    bool modified;
    QHBoxLayout* layout;
    QTreeWidget* treewidget;
    QMap<KActionCollection*,QString> actioncollections;
    QList<KKeySequenceWidget*> keysequencewidgets;
};

KShortcutsEditorPrivate::KShortcutsEditorPrivate()
    : parent(nullptr),
    actiontypes(KShortcutsEditor::AllActions),
    allowlettershortcuts(true),
    modified(false),
    layout(nullptr),
    treewidget(nullptr)
{
}

void KShortcutsEditorPrivate::init(KShortcutsEditor *_parent,
                                   const KShortcutsEditor::ActionTypes _actiontypes,
                                   const KShortcutsEditor::LetterShortcuts _lettershortcuts)
{
    parent = _parent;
    actiontypes = _actiontypes;
    allowlettershortcuts = (_lettershortcuts == KShortcutsEditor::LetterShortcutsAllowed);

    layout = new QHBoxLayout(parent);
    parent->setLayout(layout);

    // TODO: edit() override
    treewidget = new QTreeWidget(parent);
    treewidget->setSelectionMode(QAbstractItemView::SingleSelection);
    treewidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    treewidget->setColumnCount(3);
    QStringList treeheaders = QStringList()
        << i18n("Collection")
        << i18n("Local")
        << i18n("Global");
    treewidget->setHeaderLabels(treeheaders);
    treewidget->setRootIsDecorated(false);
    QHeaderView* treeheader = treewidget->header();
    treeheader->setMovable(false);
    treeheader->setStretchLastSection(false);
    treeheader->setResizeMode(0, QHeaderView::Stretch);
    treeheader->setResizeMode(1, QHeaderView::Stretch);
    treeheader->setResizeMode(2, QHeaderView::Stretch);
    treeheader->setSectionHidden(1, !(actiontypes & KShortcutsEditor::LocalAction));
    treeheader->setSectionHidden(2, !(actiontypes & KShortcutsEditor::GlobalAction));

    layout->addWidget(treewidget);
}

void KShortcutsEditorPrivate::_k_slotKeySequenceChanged()
{
    modified = true;
    emit parent->keyChange();
}


KShortcutsEditor::KShortcutsEditor(KActionCollection *collection, QWidget *parent,
                                   ActionTypes actionTypes, LetterShortcuts allowLetterShortcuts)
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->init(this, actionTypes, allowLetterShortcuts);
    addCollection(collection);
}

KShortcutsEditor::KShortcutsEditor(QWidget *parent, ActionTypes actionTypes,
                                   LetterShortcuts allowLetterShortcuts)
    : QWidget(parent),
    d(new KShortcutsEditorPrivate())
{
    d->init(this, actionTypes, allowLetterShortcuts);
}

KShortcutsEditor::~KShortcutsEditor()
{
    delete d;
}

bool KShortcutsEditor::isModified() const
{
    return d->modified;
}

void KShortcutsEditor::clearCollections()
{
    d->actioncollections.clear();
    d->treewidget->clear();
    d->keysequencewidgets.clear();
}

void KShortcutsEditor::addCollection(KActionCollection *collection, const QString &title)
{
    if (collection->isEmpty()) {
        return;
    }
    d->actioncollections.insert(collection, title);

    // all sorts of fallbacks to fill gaps
    KComponentData componentdata = collection->componentData();
    if (!componentdata.isValid()) {
        componentdata = KGlobal::mainComponent();
    }
    const KAboutData* aboutdata = componentdata.aboutData();
    QString collectionname = title;
    QString collectionicon;
    if (collectionname.isEmpty()) {
        if (aboutdata) {
            collectionname = aboutdata->programName();
        }
    }
    if (collectionname.isEmpty()) {
        collectionname = componentdata.componentName();
    }
    if (collectionname.isEmpty()) {
        collectionname = collection->objectName();
    }
    if (collectionname.isEmpty()) {
        collectionname = QString::number(quintptr(collection), 16);
    }
    if (aboutdata) {
        collectionicon = aboutdata->programIconName();
    }
    if (collectionicon.isEmpty() || KIconLoader::global()->iconPath(collectionicon, KIconLoader::Small, true).isEmpty()) {
        // for now assume it is a plugin collection, those usually have invalid program icon
        // (e.g. "katesearch") which is why it is checked above
        collectionicon = QLatin1String("preferences-plugin");
    }

    QTreeWidgetItem* topitem = nullptr;
    for (int i = 0; i < d->treewidget->topLevelItemCount(); i++) {
        QTreeWidgetItem* treeitem = d->treewidget->topLevelItem(i);
        if (treeitem->text(0) == collectionname) {
            topitem = treeitem;
            break;
        }
    }
    if (!topitem) {
        topitem = new QTreeWidgetItem(d->treewidget);
        topitem->setFlags(Qt::ItemIsEnabled);
        topitem->setText(0, collectionname);
        topitem->setIcon(0, KIcon(collectionicon));
    }
    int rowcounter = 0;
    const bool addlocal = (d->actiontypes & KShortcutsEditor::LocalAction);
    const bool addglobal = (d->actiontypes & KShortcutsEditor::GlobalAction);
    foreach (QAction *action, collection->actions()) {
        const KAction* kaction = qobject_cast<KAction*>(action);

        QTreeWidgetItem* actionitem = nullptr;

        if (addlocal && kaction && !kaction->isShortcutConfigurable()) {
            kDebug() << "local shortcut of action is not configurable" << kaction;
        } else if (addlocal) {
            if (!actionitem) {
                actionitem = kMakeActionItem(topitem, action);
            }

            KKeySequenceWidget* localkswidget = new KKeySequenceWidget(d->treewidget);
            localkswidget->setAssociatedAction(action);
            localkswidget->setModifierlessAllowed(d->allowlettershortcuts);
            localkswidget->setCheckForConflictsAgainst(
                KKeySequenceWidget::LocalShortcuts | KKeySequenceWidget::StandardShortcuts
            );
            if (kaction) {
                localkswidget->setComponentName(kaction->d->componentData.componentName());
            }
            localkswidget->setKeySequence(action->shortcut());
            localkswidget->setProperty("_k_action", QVariant::fromValue(action));
            localkswidget->setProperty("_k_global", false);
            connect(
                localkswidget, SIGNAL(keySequenceChanged(QKeySequence)),
                this, SLOT(_k_slotKeySequenceChanged())
            );
            d->treewidget->setItemWidget(actionitem, 1, localkswidget);
            d->keysequencewidgets.append(localkswidget);
        }

        if (addglobal && !kaction) {
            kWarning() << "action is not KAction" << action;
        } else if (addglobal && kaction && !kaction->isGlobalShortcutEnabled()) {
            kDebug() << "global shortcut of action is not enabled" << kaction;
        } else if (addglobal) {
            if (!actionitem) {
                actionitem = kMakeActionItem(topitem, action);
            }

            KKeySequenceWidget* globalkswidget = new KKeySequenceWidget(d->treewidget);
            globalkswidget->setAssociatedAction(action);
            globalkswidget->setModifierlessAllowed(d->allowlettershortcuts);
            globalkswidget->setCheckForConflictsAgainst(
                KKeySequenceWidget::LocalShortcuts | KKeySequenceWidget::GlobalShortcuts
                | KKeySequenceWidget::StandardShortcuts
            );
            globalkswidget->setComponentName(kaction->d->componentData.componentName());
            globalkswidget->setKeySequence(kaction->globalShortcut());
            globalkswidget->setProperty("_k_action", QVariant::fromValue(action));
            globalkswidget->setProperty("_k_global", true);
            connect(
                globalkswidget, SIGNAL(keySequenceChanged(QKeySequence)),
                this, SLOT(_k_slotKeySequenceChanged())
            );
            d->treewidget->setItemWidget(actionitem, 2, globalkswidget);
            d->keysequencewidgets.append(globalkswidget);
        }

        if (actionitem) {
            rowcounter++;
        }
    }
    topitem->sortChildren(0, Qt::AscendingOrder);
    d->treewidget->addTopLevelItem(topitem);
    topitem->setExpanded(true);

    // TODO: disable collapsing via mouse
    // force exapnsion if there is only one top-level item
    d->treewidget->setRootIsDecorated(d->treewidget->topLevelItemCount() > 1);
    // count the local and global actions, disable sections based on the count and action types
    int localcounter = 0;
    int globalcounter = 0;
    foreach (KKeySequenceWidget *kswidget, d->keysequencewidgets) {
        kswidget->setCheckActionCollections(d->actioncollections.keys());
        QAction* action = qvariant_cast<QAction*>(kswidget->property("_k_action"));
        Q_ASSERT(action != nullptr);
        const bool global = kswidget->property("_k_global").toBool();
        if (global) {
            globalcounter++;
        } else {
            localcounter++;
        }
    }
    QHeaderView* treeheader = d->treewidget->header();
    treeheader->setSectionHidden(1, !addlocal || localcounter < 1);
    treeheader->setSectionHidden(2, !addglobal || globalcounter < 1);
}

void KShortcutsEditor::importConfiguration(KConfigBase *config)
{
    if (!config) {
        config = KGlobal::config().data();
    }

    const QList<KActionCollection*> actioncollections = d->actioncollections.keys();
    if (d->actiontypes & KShortcutsEditor::LocalAction) {
        KConfigGroup group(config, "Shortcuts");
        foreach (KActionCollection* collection, actioncollections) {
            collection->readSettings(&group);
        }
    }
    if (d->actiontypes & KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Global Shortcuts");
        foreach (KActionCollection* collection, actioncollections) {
            collection->importGlobalShortcuts(&group);
        }
    }

    // start all over, it is unknown what changed in the configuration
    const QMap<KActionCollection*,QString> actioncollectionsmap = d->actioncollections;
    clearCollections();
    foreach (KActionCollection* collection, actioncollections) {
        addCollection(collection, actioncollectionsmap.value(collection));
    }
}

void KShortcutsEditor::exportConfiguration(KConfigBase *config) const
{
    if (!config) {
        config = KGlobal::config().data();
    }

    foreach (const KKeySequenceWidget *kswidget, d->keysequencewidgets) {
        QAction* action = qvariant_cast<QAction*>(kswidget->property("_k_action"));
        Q_ASSERT(action != nullptr);
        const bool global = kswidget->property("_k_global").toBool();
        if (global) {
            KAction* kaction = qobject_cast<KAction*>(action);
            Q_ASSERT(kaction != nullptr);
            kaction->setGlobalShortcut(kswidget->keySequence());
        } else {
            action->setShortcut(kswidget->keySequence());
        }
    }

    const QList<KActionCollection*> actioncollections = d->actioncollections.keys();
    if (d->actiontypes & KShortcutsEditor::LocalAction) {
        KConfigGroup group(config, "Shortcuts");
        foreach (KActionCollection* collection, actioncollections) {
            collection->writeSettings(&group, true);
        }
    }
    if (d->actiontypes & KShortcutsEditor::GlobalAction) {
        KConfigGroup group(config, "Global Shortcuts");
        foreach (KActionCollection* collection, actioncollections) {
            collection->exportGlobalShortcuts(&group, true);
        }
    }
    config->sync();

    d->modified = false;
}

void KShortcutsEditor::allDefault()
{
    foreach (KKeySequenceWidget *kswidget, d->keysequencewidgets) {
        QAction* action = qvariant_cast<QAction*>(kswidget->property("_k_action"));
        Q_ASSERT(action != nullptr);
        const bool global = kswidget->property("_k_global").toBool();
        KAction* kaction = qobject_cast<KAction*>(action);
        if (global) {
            Q_ASSERT(kaction != nullptr);
            const QKeySequence ks = kaction->globalShortcut(KAction::DefaultShortcut);
            kaction->setGlobalShortcut(ks);
            kswidget->setKeySequence(ks);
        } else {
            if (!kaction) {
                kWarning() << "cannot restore the default for action that is not KAction" << action;
                continue;
            }
            const QKeySequence ks = kaction->shortcut(KAction::DefaultShortcut);
            kaction->setShortcut(ks);
            kswidget->setKeySequence(ks);
        }
    }
    // NOTE: signal will be emitted by KKeySequenceWidget if keysequences change from a call to
    // KKeySequenceWidget::setKeySequence()
}

#include "moc_kshortcutseditor.cpp"
