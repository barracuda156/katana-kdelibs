/* This file is part of the KDE libraries
    Copyright (C) 1999 Reginald Stadlbauer <reggie@kde.org>
              (C) 1999 Simon Hausmann <hausmann@kde.org>
              (C) 2000 Nicolas Hadacek <haadcek@kde.org>
              (C) 2000 Kurt Granroth <granroth@kde.org>
              (C) 2000 Michael Koch <koch@kde.org>
              (C) 2001 Holger Freyther <freyther@kde.org>
              (C) 2002 Ellis Whitehead <ellis@kde.org>
              (C) 2002 Joseph Wenninger <jowenn@kde.org>
              (C) 2005-2007 Hamish Rodda <rodda@kde.org>

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

#include "kactioncollection.h"
#include "kactioncategory.h"
#include "kxmlguiclient.h"
#include "kxmlguifactory.h"
#include "kglobal.h"
#include "kaction.h"
#include "kaction_p.h"
#include "kcomponentdata.h"
#include "kconfiggroup.h"
#include "kdebug.h"

#include <QtXml/qdom.h>
#include <QtCore/QSet>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtGui/QAction>

#include <stdio.h>

class KActionCollectionPrivate
{
public:
    KActionCollectionPrivate()
        : m_parentGUIClient(nullptr),
        configGroup("Shortcuts"),
        connectTriggered(false),
        connectHovered(false),
        q(nullptr)
    {
    }

    void setComponentForAction(KAction *kaction)
    { kaction->d->maybeSetComponentData(componentData); }

    static QList<KActionCollection*> s_allCollections;

    void _k_associatedWidgetDestroyed(QObject *obj);
    void _k_actionDestroyed(QObject *obj);

    KComponentData componentData;

    //! Remove a action from our internal bookkeeping. Returns NULL if the
    //! action doesn't belong to us.
    QAction *unlistAction(QAction*);

    QMap<QString, QAction*> actionByName;
    QList<QAction*> actions;

    const KXMLGUIClient *m_parentGUIClient;

    QString configGroup;

    bool connectTriggered;
    bool connectHovered;

    KActionCollection *q;

    QList<QWidget*> associatedWidgets;
};

QList<KActionCollection*> KActionCollectionPrivate::s_allCollections;

KActionCollection::KActionCollection(QObject *parent, const KComponentData &cData)
    : QObject(parent),
    d(new KActionCollectionPrivate())
{
    d->q = this;
    KActionCollectionPrivate::s_allCollections.append(this);

    setComponentData(cData);
}

KActionCollection::KActionCollection(const KXMLGUIClient *parent)
    : QObject(nullptr),
    d(new KActionCollectionPrivate())
{
    d->q = this;
    KActionCollectionPrivate::s_allCollections.append(this);

    d->m_parentGUIClient = parent;
    d->componentData = parent->componentData();
}

KActionCollection::~KActionCollection()
{
    KActionCollectionPrivate::s_allCollections.removeAll(this);
    delete d;
}

void KActionCollection::clear()
{
    d->actionByName.clear();
    qDeleteAll(d->actions);
    d->actions.clear();
}

QAction* KActionCollection::action(const QString &name) const
{
    QAction* action = nullptr;
    if (!name.isEmpty()) {
        action = d->actionByName.value (name);
    }
    return action;
}

QAction* KActionCollection::action(int index) const
{
    // ### investigate if any apps use this at all
    return actions().value(index);
}

int KActionCollection::count() const
{
    return d->actions.count();
}

bool KActionCollection::isEmpty() const
{
    return count() == 0;
}

void KActionCollection::setComponentData(const KComponentData &cData)
{
    if (count() > 0) {
        // Its component name is part of an action's signature in the context of
        // global shortcuts and the semantics of changing an existing action's
        // signature are, as it seems, impossible to get right.
        // As of now this only matters for global shortcuts. We could
        // thus relax the requirement and only refuse to change the component data
        // if we have actions with global shortcuts in this collection.
        kWarning(129) << "this does not work on a KActionCollection containing actions!";
    }

    if (cData.isValid()) {
        d->componentData = cData;
    } else {
        d->componentData = KGlobal::mainComponent();
    }
}

KComponentData KActionCollection::componentData() const
{
    return d->componentData;
}

const KXMLGUIClient *KActionCollection::parentGUIClient() const
{
    return d->m_parentGUIClient;
}

QList<QAction*> KActionCollection::actions() const
{
    return d->actions;
}

const QList< QAction* > KActionCollection::actionsWithoutGroup() const
{
    QList<QAction*> ret;
    foreach (QAction* action, d->actions) {
        if (!action->actionGroup()) {
            ret.append(action);
        }
    }
    return ret;
}

const QList<QActionGroup*> KActionCollection::actionGroups() const
{
    QSet<QActionGroup*> set;
    foreach (QAction* action, d->actions) {
        if (action->actionGroup()) {
            set.insert(action->actionGroup());
        }
    }
    return set.toList();
}

KAction *KActionCollection::addAction(const QString &name, KAction *action)
{
    QAction* ret = addAction(name, static_cast<QAction*>(action));
    Q_ASSERT(ret == action);
    Q_UNUSED(ret); // fix compiler warning in release mode
    return action;
}

QAction* KActionCollection::addAction(const QString &name, QAction *action)
{
    if (!action) {
        return action;
    }

    const QString objectName = action->objectName();
    QString indexName = name;

    if (indexName.isEmpty()) {
        // No name provided. Use the objectName.
        indexName = objectName;
    } else {
        // A name was provided. Check against objectName.
        if ((!objectName.isEmpty()) && (objectName != indexName)) {
            // The user specified a new name and the action already has a
            // different one. The objectName is used for saving shortcut
            // settings to disk. Both for local and global shortcuts.
            KAction *kaction = qobject_cast<KAction*>(action);
            kDebug(125) << "Registering action " << objectName << " under new name " << indexName;
            // If there is a global shortcuts it's a very bad idea.
            if (kaction && kaction->isGlobalShortcutEnabled()) {
                // In debug mode assert
                Q_ASSERT(!kaction->isGlobalShortcutEnabled());
                // In release mode keep the old name
                kError() << "Changing action name from " << objectName << " to " << indexName << "\nignored because of active global shortcut.";
                indexName = objectName;
            }
        }

        // Set the new name
        action->setObjectName(indexName);
    }

    // No name provided and the action had no name. Make one up. This will not
    // work when trying to save shortcuts. Both local and global shortcuts.
    if (indexName.isEmpty()) {
        indexName = indexName.sprintf("unnamed-%p", (void*)action);
        action->setObjectName(indexName);
    }

    // From now on the objectName has to have a value. Else we cannot safely
    // remove actions.
    Q_ASSERT(!action->objectName().isEmpty());

    // look if we already have THIS action under THIS name ;)
    if (d->actionByName.value(indexName, 0) == action) {
        // This is not a multi map!
        Q_ASSERT(d->actionByName.count(indexName) == 1);
        return action;
    }

    // Check if we have another action under this name
    if (QAction *oldAction = d->actionByName.value(indexName)) {
        takeAction(oldAction);
    }

    // Check if we have this action under a different name.
    // Not using takeAction because we don't want to remove it from categories,
    // and because it has the new name already.
    const int oldIndex = d->actions.indexOf(action);
    if (oldIndex != -1) {
        d->actionByName.remove(d->actionByName.key(action));
        d->actions.removeAt(oldIndex);
    }

    // Add action to our lists.
    d->actionByName.insert(indexName, action);
    d->actions.append(action);

    foreach (QWidget *widget, d->associatedWidgets) {
        widget->addAction(action);
    }

    connect(action, SIGNAL(destroyed(QObject*)), SLOT(_k_actionDestroyed(QObject*)));

    // only our private class is a friend of KAction
    if (KAction *kaction = qobject_cast<KAction *>(action)) {
      d->setComponentForAction(kaction);
    }

    if (d->connectHovered) {
        connect(action, SIGNAL(hovered()), SLOT(slotActionHovered()));
    }

    if (d->connectTriggered) {
        connect(action, SIGNAL(triggered(bool)), SLOT(slotActionTriggered()));
    }

    emit inserted(action);
    return action;
}

void KActionCollection::removeAction(QAction *action)
{
    delete takeAction(action);
}

QAction* KActionCollection::takeAction(QAction *action)
{
    if (!d->unlistAction(action)) {
        return nullptr;
    }

    // Remove the action from all widgets
    foreach (QWidget *widget, d->associatedWidgets) {
        widget->removeAction(action);
    }

    action->disconnect(this);
    return action;
}

KAction* KActionCollection::addAction(KStandardAction::StandardAction actionType, const QObject *receiver, const char *member)
{
    return KStandardAction::create(actionType, receiver, member, this);
}

KAction *KActionCollection::addAction(KStandardAction::StandardAction actionType, const QString &name,
                                      const QObject *receiver, const char *member)
{
    // pass 0 as parent, because if the parent is a KActionCollection KStandardAction::create automatically
    // adds the action to it under the default name. We would trigger the
    // warning about renaming the action then.
    KAction *action = KStandardAction::create(actionType, receiver, member, 0);
    // Give it a parent for gc.
    action->setParent(this);
    // Remove the name to get rid of the "rename action" warning above
    action->setObjectName(name);
    // And now add it with the desired name.
    return addAction(name, action);
}

KAction* KActionCollection::addAction(const QString &name, const QObject *receiver, const char *member)
{
    KAction *a = new KAction(this);
    if (receiver && member) {
        connect(a, SIGNAL(triggered(bool)), receiver, member);
    }
    return addAction(name, a);
}

QString KActionCollection::configGroup() const
{
    return d->configGroup;
}

void KActionCollection::setConfigGroup(const QString &group)
{
    d->configGroup = group;
}

void KActionCollection::readSettings(KConfigGroup *config)
{
    KConfigGroup cg;
    if (d->componentData.isValid()) {
        cg = KConfigGroup(d->componentData.config(), configGroup());
    } else {
        cg = KConfigGroup(KGlobal::config(), configGroup());
    }
    if (!config) {
        config = &cg;
    }

    if (!config->exists()) {
        return;
    }

    for (QMap<QString, QAction*>::ConstIterator it = d->actionByName.constBegin();
        it != d->actionByName.constEnd(); ++it) {
        KAction *kaction = qobject_cast<KAction*>(it.value());
        if (!kaction) {
            continue;
        }

        if (kaction->isShortcutConfigurable() ) {
            const QString actionName = it.key();
            const QString entry = config->readEntry(actionName, QString());
            kDebug(125) << "reading" << actionName << " = " << entry;
            if (!entry.isEmpty()) {
                kaction->setShortcut(QKeySequence(entry), KAction::ActiveShortcut);
            } else {
                kaction->setShortcut(kaction->shortcut(KAction::DefaultShortcut), KAction::ActiveShortcut);
            }
        }

        if (kaction->isShortcutConfigurable() && kaction->isGlobalShortcutEnabled()) {
            const QString globalActionName = it.key() + QLatin1String("_global");
            const QString entry = config->readEntry(globalActionName, QString());
            kDebug(125) << "reading global" << globalActionName << " = " << entry;
            if (!entry.isEmpty()) {
                kaction->setGlobalShortcut(QKeySequence(entry), KAction::ActiveShortcut);
            } else {
                kaction->setGlobalShortcut(kaction->shortcut(KAction::DefaultShortcut), KAction::ActiveShortcut);
            }
        }
    }

    // kDebug(125) << "done";
}

void KActionCollection::writeSettings(KConfigGroup *config, bool writeAll, QAction *oneAction) const
{
    KConfigGroup cg;
    if (d->componentData.isValid()) {
        cg = KConfigGroup(d->componentData.config(), configGroup());
    } else {
        cg = KConfigGroup(KGlobal::config(), configGroup());
    }
    if (!config) {
        config = &cg;
    }

    QList<QAction*> writeActions;
    if (oneAction) {
        writeActions.append(oneAction);
    } else {
        writeActions = actions();
    }

    for (QMap<QString, QAction *>::ConstIterator it = d->actionByName.constBegin();
         it != d->actionByName.constEnd(); ++it) {

        // Get the action. We only handle KActions so skip QActions
        KAction *kaction = qobject_cast<KAction*>(it.value());
        if (!kaction) {
            continue;
        }

        const QString actionName = it.key();

        // If the action name starts with unnamed- spit out a warning and ignore
        // it. That name will change at will and will break loading writing
        if (actionName.startsWith(QLatin1String("unnamed-"))) {
            kError() << "Skipped saving Shortcut for action without name " << kaction->text() << "!";
            continue;
        }

        // Write the shortcut
        if (kaction->isShortcutConfigurable()) {
            bool bConfigHasAction = !config->readEntry(actionName, QString()).isEmpty();
            bool bSameAsDefault = (kaction->shortcut() == kaction->shortcut(KAction::DefaultShortcut));
            if (writeAll || !bSameAsDefault) {
                // We are instructed to write all shortcuts or the shortcut is
                // not set to its default value. Write it
                const QString s = kaction->shortcut().toString();
                kDebug(125) << "writing " << actionName << " = " << s;
                config->writeEntry(actionName, s);

            } else if (bConfigHasAction) {
                // Otherwise, this key is the same as default but exists in
                // config file. Remove it.
                kDebug(125) << "removing " << actionName << " because == default";
                config->deleteEntry(actionName);
            }
        }

        if (kaction->isShortcutConfigurable() && kaction->isGlobalShortcutEnabled()) {
            const QString globalActionName = actionName + QLatin1String("_global");
            bool bConfigHasAction = !config->readEntry(globalActionName, QString()).isEmpty();
            bool bSameAsDefault = (kaction->globalShortcut() == kaction->globalShortcut(KAction::DefaultShortcut));
            if (writeAll || !bSameAsDefault) {
                const QString s = kaction->globalShortcut().toString();
                kDebug(125) << "writing " << globalActionName << " = " << s;
                config->writeEntry(globalActionName, s);
            } else if (bConfigHasAction) {
                // Otherwise, this key is the same as default but exists in config file.  Remove it.
              kDebug(125) << "removing " << globalActionName << " because == default";
              config->deleteEntry( globalActionName );
            }
        }
    }

    config->sync();
}

void KActionCollection::slotActionTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        emit actionTriggered(action);
    }
}

void KActionCollection::slotActionHovered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        emit actionHovered(action);
    }
}

void KActionCollectionPrivate::_k_actionDestroyed(QObject *obj)
{
    // obj isn't really a QAction anymore. So make sure we don't do fancy stuff
    // with it.
    QAction *action = static_cast<QAction*>(obj);

    unlistAction(action);
}

void KActionCollection::connectNotify(const char *signal)
{
    if (d->connectHovered && d->connectTriggered) {
        return;
    }

    if (QMetaObject::normalizedSignature(SIGNAL(actionHovered(QAction*))) == signal) {
        if (!d->connectHovered) {
            d->connectHovered = true;
            foreach (QAction* action, actions()) {
                connect(action, SIGNAL(hovered()), SLOT(slotActionHovered()));
            }
        }
    } else if (QMetaObject::normalizedSignature(SIGNAL(actionTriggered(QAction*))) == signal) {
        if (!d->connectTriggered) {
            d->connectTriggered = true;
            foreach (QAction *action, actions()) {
                connect(action, SIGNAL(triggered(bool)), SLOT(slotActionTriggered()));
            }
        }
    }

    QObject::connectNotify(signal);
}

const QList< KActionCollection*>& KActionCollection::allCollections()
{
    return KActionCollectionPrivate::s_allCollections;
}

void KActionCollection::associateWidget(QWidget *widget) const
{
    foreach (QAction *action, actions()) {
        if (!widget->actions().contains(action)) {
            widget->addAction(action);
        }
    }
}

void KActionCollection::addAssociatedWidget(QWidget *widget)
{
    if (!d->associatedWidgets.contains(widget)) {
        widget->addActions(actions());

        d->associatedWidgets.append(widget);
        connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(_k_associatedWidgetDestroyed(QObject*)));
    }
}

void KActionCollection::removeAssociatedWidget(QWidget *widget)
{
    foreach (QAction* action, actions()) {
        widget->removeAction(action);
    }

    d->associatedWidgets.removeAll(widget);
    disconnect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(_k_associatedWidgetDestroyed(QObject*)));
}

QAction* KActionCollectionPrivate::unlistAction(QAction *action)
{
    // ATTENTION:
    //   This method is called with an QObject formerly known as a QAction
    //   during _k_actionDestroyed(). So don't do fancy stuff here that needs a
    //   real QAction!

    // Get the index for the action
    const int index = actions.indexOf(action);

    // Action not found.
    if (index==-1) {
        return nullptr;
    }

    // An action collection can't have the same action twice.
    Q_ASSERT(actions.indexOf(action,index + 1) == -1);

    // Get the actions name
    const QString name = action->objectName();

    // Remove the action
    actionByName.remove(name);
    actions.removeAt(index);

    // Remove the action from the categories. Should be only one
    QList<KActionCategory*> categories = q->findChildren<KActionCategory*>();
    foreach (KActionCategory *category, categories) {
        category->unlistAction(action);
    }

    return action;
}

QList<QWidget*> KActionCollection::associatedWidgets() const
{
    return d->associatedWidgets;
}

void KActionCollection::clearAssociatedWidgets()
{
    foreach (QWidget* widget, d->associatedWidgets) {
        foreach (QAction* action, actions()) {
            widget->removeAction(action);
        }
    }
    d->associatedWidgets.clear();
}

void KActionCollectionPrivate::_k_associatedWidgetDestroyed(QObject *obj)
{
    associatedWidgets.removeAll(static_cast<QWidget*>(obj));
}

#include "moc_kactioncollection.cpp"
