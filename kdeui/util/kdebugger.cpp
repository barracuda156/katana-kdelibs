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

#include "kdebugger.h"
#include "ktreewidgetsearchline.h"
#include "kpushbutton.h"
#include "ktabwidget.h"
#include "ktextedit.h"
#include "klocale.h"
#include "kdebug.h"

#include <QApplication>
#include <QGridLayout>
#include <QTreeWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMetaProperty>

Q_DECLARE_METATYPE(QObject*);

static QString kObjectString(const QObject *object)
{
    const QString objectname = object->objectName();
    if (!objectname.isEmpty()) {
        return objectname;
    }
    const QString classname = object->metaObject()->className();
    if (!classname.isEmpty()) {
        return classname;
    }
    return QString::number(quintptr(object), 16);
}

static void kSetPropertiesHeaders(QTableWidget *propertieswidget)
{
    const QStringList tableheaders = QStringList()
        << i18n("Property")
        << i18n("Value");
    propertieswidget->setHorizontalHeaderLabels(tableheaders);
}

class KDebuggerPrivate : public QObject
{
    Q_OBJECT
public:
    KDebuggerPrivate(QObject *parent = nullptr);

    QWidget* mainwidget;
    QGridLayout* mainlayout;
    KTreeWidgetSearchLine* objectssearchline;
    KPushButton* objectsrefreshbutton;
    QTreeWidget* objectswidget;
    KTabWidget* tabwidget;
    KTextEdit* eventsedit;
    QTableWidget* propertieswidget;

public Q_SLOTS:
    void slotUpdateObjects();
    void slotItemSelectionChanged();
    void slotItemChanged(QTableWidgetItem *propertyvalueitem);

protected:
    bool eventFilter(QObject *object, QEvent *event) final;

private:
    void addObject(QObject *object, QTreeWidgetItem *parentitem);

    QPointer<QObject> m_object;
};

KDebuggerPrivate::KDebuggerPrivate(QObject *parent)
    : QObject(parent),
    mainwidget(nullptr),
    mainlayout(nullptr),
    objectssearchline(nullptr),
    objectsrefreshbutton(nullptr),
    objectswidget(nullptr),
    tabwidget(nullptr),
    eventsedit(nullptr),
    propertieswidget(nullptr)
{
}

void KDebuggerPrivate::addObject(QObject *object, QTreeWidgetItem *parentitem)
{
    QTreeWidgetItem* objectitem = new QTreeWidgetItem(parentitem);
    objectitem->setText(0, kObjectString(object));
    objectitem->setData(0, Qt::UserRole, QVariant::fromValue(object));
    objectitem->setExpanded(true);
    foreach (QObject *childobject, object->children()) {
        addObject(childobject, objectitem);
    }
}

bool KDebuggerPrivate::eventFilter(QObject *object, QEvent *event)
{
    switch (event->type()) {
        // TODO: option for what events to show
        case QEvent::Timer:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::Paint:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Create:
        case QEvent::Destroy:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Close:
        case QEvent::Quit:
        case QEvent::ParentChange:
        case QEvent::ParentAboutToChange:
        case QEvent::ThreadChange:
        case QEvent::WindowActivate:
        case QEvent::WindowDeactivate:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::Wheel:
        case QEvent::WindowTitleChange:
        case QEvent::WindowIconChange:
        case QEvent::ApplicationWindowIconChange:
        case QEvent::ApplicationFontChange:
        case QEvent::ApplicationLayoutDirectionChange:
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::Clipboard:
        case QEvent::MetaCall:
        case QEvent::SockAct:
        case QEvent::DeferredDelete:
        case QEvent::DragEnter:
        case QEvent::DragMove:
        case QEvent::DragLeave:
        case QEvent::Drop:
        case QEvent::ChildAdded:
        case QEvent::ChildPolished:
        case QEvent::ChildRemoved:
        case QEvent::ShowWindowRequest:
        case QEvent::PolishRequest:
        case QEvent::Polish:
        case QEvent::LayoutRequest:
        case QEvent::UpdateRequest:
        case QEvent::UpdateLater:
        case QEvent::ContextMenu:
        case QEvent::LocaleChange:
        case QEvent::LanguageChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::Style:
        case QEvent::FontChange:
        case QEvent::EnabledChange:
        case QEvent::ActivationChange:
        case QEvent::StyleChange:
        case QEvent::IconTextChange:
        case QEvent::ModifiedChange:
        case QEvent::MouseTrackingChange:
        case QEvent::WindowBlocked:
        case QEvent::WindowUnblocked:
        case QEvent::WindowStateChange:
        case QEvent::ToolTip:
        case QEvent::WhatsThis:
        case QEvent::StatusTip:
        case QEvent::ActionChanged:
        case QEvent::ActionAdded:
        case QEvent::ActionRemoved:
        case QEvent::Shortcut:
        case QEvent::ShortcutOverride:
        case QEvent::WhatsThisClicked:
        case QEvent::ApplicationActivate:
        case QEvent::ApplicationDeactivate:
        case QEvent::QueryWhatsThis:
        case QEvent::EnterWhatsThisMode:
        case QEvent::LeaveWhatsThisMode:
        case QEvent::ZOrderChange:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        case QEvent::AcceptDropsChange:
        case QEvent::GraphicsSceneMouseMove:
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneContextMenu:
        case QEvent::GraphicsSceneHoverEnter:
        case QEvent::GraphicsSceneHoverMove:
        case QEvent::GraphicsSceneHoverLeave:
        case QEvent::GraphicsSceneLeave:
        case QEvent::GraphicsSceneHelp:
        case QEvent::GraphicsSceneDragEnter:
        case QEvent::GraphicsSceneDragMove:
        case QEvent::GraphicsSceneDragLeave:
        case QEvent::GraphicsSceneDrop:
        case QEvent::GraphicsSceneWheel:
        case QEvent::KeyboardLayoutChange:
        case QEvent::DynamicPropertyChange:
        case QEvent::ContentsRectChange:
        case QEvent::GraphicsSceneResize:
        case QEvent::GraphicsSceneMove:
        case QEvent::CursorChange:
        case QEvent::ToolTipChange:
        case QEvent::GrabMouse:
        case QEvent::UngrabMouse:
        case QEvent::GrabKeyboard:
        case QEvent::UngrabKeyboard:
        case QEvent::RequestSoftwareInputPanel:
        case QEvent::CloseSoftwareInputPanel:
        case QEvent::WinIdChange: {
            QString eventline = kObjectString(object);
            eventline.append(QLatin1String(" - "));
            eventline.append(QString::number(event->type()));
            eventsedit->append(eventline);
            break;
        }
        default: {
            break;
        }
    }
    return false;
}

void KDebuggerPrivate::slotUpdateObjects()
{
    objectswidget->clear();
    addObject(qApp, objectswidget->invisibleRootItem());
    foreach (QWidget *widget, QApplication::allWidgets()) {
        addObject(widget, objectswidget->invisibleRootItem());
    }
}

void KDebuggerPrivate::slotItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedobjects = objectswidget->selectedItems();
    if (selectedobjects.isEmpty()) {
        return;
    }
    QTreeWidgetItem* objectitem = selectedobjects.first();
    QObject* object = qvariant_cast<QObject*>(objectitem->data(0, Qt::UserRole));
    if (!object) {
        return;
    }
    if (m_object) {
        m_object->removeEventFilter(this);
    }
    m_object = object;
    kDebug() << "watching" << m_object;
    eventsedit->clear();
    m_object->installEventFilter(this);

    propertieswidget->clear();
    propertieswidget->blockSignals(true);
    // this has to be done after every clear
    kSetPropertiesHeaders(propertieswidget);
    int propertiesrowcount = 0;
    const QMetaObject* metaobject = m_object->metaObject();
    for (int i = 0; i < metaobject->propertyCount(); i++) {
        propertieswidget->setRowCount(propertiesrowcount + 1);
        const QMetaProperty metaproperty = metaobject->property(i);
        QTableWidgetItem* propertynameitem = new QTableWidgetItem(metaproperty.name());
        propertynameitem->setFlags(Qt::ItemIsEnabled);
        propertieswidget->setItem(propertiesrowcount, 0, propertynameitem);
        QTableWidgetItem* propertyvalueitem = new QTableWidgetItem(metaproperty.read(m_object).toString());
        Qt::ItemFlags propertyvalueflags = (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (metaproperty.isWritable()) {
            propertyvalueflags |= Qt::ItemIsEditable;
        }
        propertyvalueitem->setFlags(propertyvalueflags);
        propertyvalueitem->setData(Qt::UserRole, i);
        propertieswidget->setItem(propertiesrowcount, 1, propertyvalueitem);
        propertiesrowcount++;
    }
    // TODO: dynamic properties
    // qDebug() << Q_FUNC_INFO << m_object->dynamicPropertyNames();
    propertieswidget->blockSignals(false);
}

void KDebuggerPrivate::slotItemChanged(QTableWidgetItem *propertyvalueitem)
{
    if (!m_object) {
        return;
    }
    const int propertyindex = propertyvalueitem->data(Qt::UserRole + 1).toInt();
    const QMetaObject* metaobject = m_object->metaObject();
    QMetaProperty metaproperty = metaobject->property(propertyindex);
    metaproperty.write(m_object, propertyvalueitem->text());
}


KDebugger::KDebugger(QWidget *parent)
    : KDialog(parent),
    d(new KDebuggerPrivate())
{
    setCaption(i18nc("@title:window", "Debugger"));
    setButtons(KDialog::Close);

    d->mainwidget = new QWidget(this);
    d->mainlayout = new QGridLayout(d->mainwidget);
    d->mainwidget->setLayout(d->mainlayout);
    setMainWidget(d->mainwidget);

    d->objectssearchline = new KTreeWidgetSearchLine(d->mainwidget);
    d->mainlayout->addWidget(d->objectssearchline, 0, 0, 1, 1);
    d->objectsrefreshbutton = new KPushButton(d->mainwidget);
    d->objectsrefreshbutton->setIcon(KIcon("view-refresh"));
    connect(
        d->objectsrefreshbutton, SIGNAL(clicked()),
        d, SLOT(slotUpdateObjects())
    );
    d->mainlayout->addWidget(d->objectsrefreshbutton, 0, 1, 1, 1);

    d->objectswidget = new QTreeWidget(d->mainwidget);
    QHeaderView* objectsheader = d->objectswidget->header();
    objectsheader->setVisible(false);
    d->objectssearchline->addTreeWidget(d->objectswidget);
    d->mainlayout->addWidget(d->objectswidget, 1, 0, 1, 2);

    d->tabwidget = new KTabWidget(d->mainwidget);
    d->mainlayout->addWidget(d->tabwidget, 0, 2, 2, 1);

    d->eventsedit = new KTextEdit(d->tabwidget);
    d->eventsedit->setReadOnly(false);
    d->tabwidget->addTab(d->eventsedit, KIcon("utilities-log-viewer"), i18n("Events"));

    d->propertieswidget = new QTableWidget(d->tabwidget);
    d->propertieswidget->setSelectionMode(QAbstractItemView::SingleSelection);
    d->propertieswidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    d->propertieswidget->setColumnCount(2);
    kSetPropertiesHeaders(d->propertieswidget);
    QHeaderView* verticalheader = d->propertieswidget->verticalHeader();
    verticalheader->setVisible(false);
    QHeaderView* horizontalheader = d->propertieswidget->horizontalHeader();
    horizontalheader->setMovable(false);
    horizontalheader->setStretchLastSection(false);
    horizontalheader->setResizeMode(0, QHeaderView::Stretch);
    horizontalheader->setResizeMode(1, QHeaderView::Stretch);
    d->tabwidget->addTab(d->propertieswidget, KIcon("document-properties"), i18n("Properties"));

    KConfigGroup kconfiggroup(KGlobal::config(), "Debugger");
    restoreDialogSize(kconfiggroup);

    d->slotUpdateObjects();

    connect(
        d->objectswidget, SIGNAL(itemSelectionChanged()),
        d, SLOT(slotItemSelectionChanged())
    );
    connect(
        d->propertieswidget, SIGNAL(itemChanged(QTableWidgetItem*)),
        d, SLOT(slotItemChanged(QTableWidgetItem*))
    );
}

KDebugger::~KDebugger()
{
    KConfigGroup kconfiggroup(KGlobal::config(), "Debugger");
    saveDialogSize(kconfiggroup);
    KGlobal::config()->sync();

    delete d;
}

#include "moc_kdebugger.cpp"
#include "kdebugger.moc"