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
#include "kvbox.h"
#include "ktimeedit.h"
#include "kthreadpool.h"
#include "krandom.h"
#include "klocale.h"
#include "kdebug.h"

#include <QApplication>
#include <QGridLayout>
#include <QTreeWidget>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMetaProperty>
#include <QElapsedTimer>
#include <QMutex>
#include <QTimer>

Q_DECLARE_METATYPE(QObject*);

// any event that does not require specialized event class for now and may not cause havoc while
// testing the fuzzer
static const QEvent::Type fuzzeventtypes[] =
{
    QEvent::Show,
    QEvent::Hide
};
static const int fuzzeventtypessize = 2;

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

static QString kEventString(const QEvent *event)
{
    QString result;
    switch (event->type()) {
        case QEvent::Timer: {
            result = QLatin1String("Timer");
            break;
        }
        case QEvent::MouseButtonPress: {
            result = QLatin1String("MouseButtonPress");
            break;
        }
        case QEvent::MouseButtonRelease: {
            result = QLatin1String("MouseButtonRelease");
            break;
        }
        case QEvent::MouseButtonDblClick: {
            result = QLatin1String("MouseButtonDblClick");
            break;
        }
        case QEvent::MouseMove: {
            result = QLatin1String("MouseMove");
            break;
        }
        case QEvent::KeyPress: {
            result = QLatin1String("KeyPress");
            break;
        }
        case QEvent::KeyRelease: {
            result = QLatin1String("KeyRelease");
            break;
        }
        case QEvent::FocusIn: {
            result = QLatin1String("FocusIn");
            break;
        }
        case QEvent::FocusOut: {
            result = QLatin1String("FocusOut");
            break;
        }
        case QEvent::Enter: {
            result = QLatin1String("Enter");
            break;
        }
        case QEvent::Leave: {
            result = QLatin1String("Leave");
            break;
        }
        case QEvent::Paint: {
            result = QLatin1String("Paint");
            break;
        }
        case QEvent::Move: {
            result = QLatin1String("Move");
            break;
        }
        case QEvent::Resize: {
            result = QLatin1String("Resize");
            break;
        }
        case QEvent::Create: {
            result = QLatin1String("Create");
            break;
        }
        case QEvent::Destroy: {
            result = QLatin1String("Destroy");
            break;
        }
        case QEvent::Show: {
            result = QLatin1String("Show");
            break;
        }
        case QEvent::Hide: {
            result = QLatin1String("Hide");
            break;
        }
        case QEvent::Close: {
            result = QLatin1String("Close");
            break;
        }
        case QEvent::Quit: {
            result = QLatin1String("Quit");
            break;
        }
        case QEvent::ParentChange: {
            result = QLatin1String("ParentChange");
            break;
        }
        case QEvent::ParentAboutToChange: {
            result = QLatin1String("ParentAboutToChange");
            break;
        }
        case QEvent::ThreadChange: {
            result = QLatin1String("ThreadChange");
            break;
        }
        case QEvent::WindowActivate: {
            result = QLatin1String("WindowActivate");
            break;
        }
        case QEvent::WindowDeactivate: {
            result = QLatin1String("WindowDeactivate");
            break;
        }
        case QEvent::ShowToParent: {
            result = QLatin1String("ShowToParent");
            break;
        }
        case QEvent::HideToParent: {
            result = QLatin1String("HideToParent");
            break;
        }
        case QEvent::Wheel: {
            result = QLatin1String("Wheel");
            break;
        }
        case QEvent::WindowTitleChange: {
            result = QLatin1String("WindowTitleChange");
            break;
        }
        case QEvent::WindowIconChange: {
            result = QLatin1String("WindowIconChange");
            break;
        }
        case QEvent::ApplicationWindowIconChange: {
            result = QLatin1String("ApplicationWindowIconChange");
            break;
        }
        case QEvent::ApplicationFontChange: {
            result = QLatin1String("ApplicationFontChange");
            break;
        }
        case QEvent::ApplicationLayoutDirectionChange: {
            result = QLatin1String("ApplicationLayoutDirectionChange");
            break;
        }
        case QEvent::ApplicationPaletteChange: {
            result = QLatin1String("ApplicationPaletteChange");
            break;
        }
        case QEvent::PaletteChange: {
            result = QLatin1String("PaletteChange");
            break;
        }
        case QEvent::Clipboard: {
            result = QLatin1String("Clipboard");
            break;
        }
        case QEvent::MetaCall: {
            result = QLatin1String("MetaCall");
            break;
        }
        case QEvent::SockAct: {
            result = QLatin1String("SockAct");
            break;
        }
        case QEvent::DeferredDelete: {
            result = QLatin1String("DeferredDelete");
            break;
        }
        case QEvent::DragEnter: {
            result = QLatin1String("DragEnter");
            break;
        }
        case QEvent::DragMove: {
            result = QLatin1String("DragMove");
            break;
        }
        case QEvent::DragLeave: {
            result = QLatin1String("DragLeave");
            break;
        }
        case QEvent::Drop: {
            result = QLatin1String("Drop");
            break;
        }
        case QEvent::ChildAdded: {
            result = QLatin1String("ChildAdded");
            break;
        }
        case QEvent::ChildPolished: {
            result = QLatin1String("ChildPolished");
            break;
        }
        case QEvent::ChildRemoved: {
            result = QLatin1String("ChildRemoved");
            break;
        }
        case QEvent::ShowWindowRequest: {
            result = QLatin1String("ShowWindowRequest");
            break;
        }
        case QEvent::PolishRequest: {
            result = QLatin1String("PolishRequest");
            break;
        }
        case QEvent::Polish: {
            result = QLatin1String("Polish");
            break;
        }
        case QEvent::LayoutRequest: {
            result = QLatin1String("LayoutRequest");
            break;
        }
        case QEvent::UpdateRequest: {
            result = QLatin1String("UpdateRequest");
            break;
        }
        case QEvent::UpdateLater: {
            result = QLatin1String("UpdateLater");
            break;
        }
        case QEvent::ContextMenu: {
            result = QLatin1String("ContextMenu");
            break;
        }
        case QEvent::LocaleChange: {
            result = QLatin1String("LocaleChange");
            break;
        }
        case QEvent::LanguageChange: {
            result = QLatin1String("LanguageChange");
            break;
        }
        case QEvent::LayoutDirectionChange: {
            result = QLatin1String("LayoutDirectionChange");
            break;
        }
        case QEvent::Style: {
            result = QLatin1String("Style");
            break;
        }
        case QEvent::FontChange: {
            result = QLatin1String("FontChange");
            break;
        }
        case QEvent::EnabledChange: {
            result = QLatin1String("EnabledChange");
            break;
        }
        case QEvent::ActivationChange: {
            result = QLatin1String("ActivationChange");
            break;
        }
        case QEvent::StyleChange: {
            result = QLatin1String("StyleChange");
            break;
        }
        case QEvent::IconTextChange: {
            result = QLatin1String("IconTextChange");
            break;
        }
        case QEvent::ModifiedChange: {
            result = QLatin1String("ModifiedChange");
            break;
        }
        case QEvent::MouseTrackingChange: {
            result = QLatin1String("MouseTrackingChange");
            break;
        }
        case QEvent::WindowBlocked: {
            result = QLatin1String("WindowBlocked");
            break;
        }
        case QEvent::WindowUnblocked: {
            result = QLatin1String("WindowUnblocked");
            break;
        }
        case QEvent::WindowStateChange: {
            result = QLatin1String("WindowStateChange");
            break;
        }
        case QEvent::ToolTip: {
            result = QLatin1String("ToolTip");
            break;
        }
        case QEvent::WhatsThis: {
            result = QLatin1String("WhatsThis");
            break;
        }
        case QEvent::StatusTip: {
            result = QLatin1String("StatusTip");
            break;
        }
        case QEvent::ActionChanged: {
            result = QLatin1String("ActionChanged");
            break;
        }
        case QEvent::ActionAdded: {
            result = QLatin1String("ActionAdded");
            break;
        }
        case QEvent::ActionRemoved: {
            result = QLatin1String("ActionRemoved");
            break;
        }
        case QEvent::Shortcut: {
            result = QLatin1String("Shortcut");
            break;
        }
        case QEvent::ShortcutOverride: {
            result = QLatin1String("ShortcutOverride");
            break;
        }
        case QEvent::WhatsThisClicked: {
            result = QLatin1String("WhatsThisClicked");
            break;
        }
        case QEvent::ApplicationActivate: {
            result = QLatin1String("ApplicationActivate");
            break;
        }
        case QEvent::ApplicationDeactivate: {
            result = QLatin1String("ApplicationDeactivate");
            break;
        }
        case QEvent::QueryWhatsThis: {
            result = QLatin1String("QueryWhatsThis");
            break;
        }
        case QEvent::EnterWhatsThisMode: {
            result = QLatin1String("EnterWhatsThisMode");
            break;
        }
        case QEvent::LeaveWhatsThisMode: {
            result = QLatin1String("LeaveWhatsThisMode");
            break;
        }
        case QEvent::ZOrderChange: {
            result = QLatin1String("ZOrderChange");
            break;
        }
        case QEvent::HoverEnter: {
            result = QLatin1String("HoverEnter");
            break;
        }
        case QEvent::HoverLeave: {
            result = QLatin1String("HoverLeave");
            break;
        }
        case QEvent::HoverMove: {
            result = QLatin1String("HoverMove");
            break;
        }
        case QEvent::AcceptDropsChange: {
            result = QLatin1String("AcceptDropsChange");
            break;
        }
        case QEvent::GraphicsSceneMouseMove: {
            result = QLatin1String("GraphicsSceneMouseMove");
            break;
        }
        case QEvent::GraphicsSceneMousePress: {
            result = QLatin1String("GraphicsSceneMousePress");
            break;
        }
        case QEvent::GraphicsSceneMouseRelease: {
            result = QLatin1String("GraphicsSceneMouseRelease");
            break;
        }
        case QEvent::GraphicsSceneMouseDoubleClick: {
            result = QLatin1String("GraphicsSceneMouseDoubleClick");
            break;
        }
        case QEvent::GraphicsSceneContextMenu: {
            result = QLatin1String("GraphicsSceneContextMenu");
            break;
        }
        case QEvent::GraphicsSceneHoverEnter: {
            result = QLatin1String("GraphicsSceneHoverEnter");
            break;
        }
        case QEvent::GraphicsSceneHoverMove: {
            result = QLatin1String("GraphicsSceneHoverMove");
            break;
        }
        case QEvent::GraphicsSceneHoverLeave: {
            result = QLatin1String("GraphicsSceneHoverLeave");
            break;
        }
        case QEvent::GraphicsSceneLeave: {
            result = QLatin1String("GraphicsSceneLeave");
            break;
        }
        case QEvent::GraphicsSceneHelp: {
            result = QLatin1String("GraphicsSceneHelp");
            break;
        }
        case QEvent::GraphicsSceneDragEnter: {
            result = QLatin1String("GraphicsSceneDragEnter");
            break;
        }
        case QEvent::GraphicsSceneDragMove: {
            result = QLatin1String("GraphicsSceneDragMove");
            break;
        }
        case QEvent::GraphicsSceneDragLeave: {
            result = QLatin1String("GraphicsSceneDragLeave");
            break;
        }
        case QEvent::GraphicsSceneDrop: {
            result = QLatin1String("GraphicsSceneDrop");
            break;
        }
        case QEvent::GraphicsSceneWheel: {
            result = QLatin1String("GraphicsSceneWheel");
            break;
        }
        case QEvent::KeyboardLayoutChange: {
            result = QLatin1String("KeyboardLayoutChange");
            break;
        }
        case QEvent::DynamicPropertyChange: {
            result = QLatin1String("DynamicPropertyChange");
            break;
        }
        case QEvent::ContentsRectChange: {
            result = QLatin1String("ContentsRectChange");
            break;
        }
        case QEvent::GraphicsSceneResize: {
            result = QLatin1String("GraphicsSceneResize");
            break;
        }
        case QEvent::GraphicsSceneMove: {
            result = QLatin1String("GraphicsSceneMove");
            break;
        }
        case QEvent::CursorChange: {
            result = QLatin1String("CursorChange");
            break;
        }
        case QEvent::ToolTipChange: {
            result = QLatin1String("ToolTipChange");
            break;
        }
        case QEvent::GrabMouse: {
            result = QLatin1String("GrabMouse");
            break;
        }
        case QEvent::UngrabMouse: {
            result = QLatin1String("UngrabMouse");
            break;
        }
        case QEvent::GrabKeyboard: {
            result = QLatin1String("GrabKeyboard");
            break;
        }
        case QEvent::UngrabKeyboard: {
            result = QLatin1String("UngrabKeyboard");
            break;
        }
        case QEvent::RequestSoftwareInputPanel: {
            result = QLatin1String("RequestSoftwareInputPanel");
            break;
        }
        case QEvent::CloseSoftwareInputPanel: {
            result = QLatin1String("CloseSoftwareInputPanel");
            break;
        }
        case QEvent::WinIdChange: {
            result = QLatin1String("WinIdChange");
            break;
        }
        default: {
            result = QLatin1String("Unknown");
            break;
        }
    }
    result.append(QLatin1String(" ("));
    result.append(QString::number(event->type()));
    result.append(QLatin1String(")"));
    return result;
}

class KDebuggerFuzzer : public QThread
{
    Q_OBJECT
public:
    KDebuggerFuzzer(QObject *parent,
                    QObject *object, const qint64 duration,
                    const bool fuzzevents, const bool fuzzproperties);

Q_SIGNALS:
    void message(const QString &message);

protected:
    void run() final;

private:
    QPointer<QObject> m_object;
    const qint64 m_duration;
    const bool m_fuzzevents;
    const bool m_fuzzproperties;
};

KDebuggerFuzzer::KDebuggerFuzzer(QObject *parent,
                                 QObject *object, const qint64 duration,
                                 const bool fuzzevents, const bool fuzzproperties)
    : QThread(parent),
    m_object(object),
    m_duration(duration),
    m_fuzzevents(fuzzevents),
    m_fuzzproperties(fuzzproperties)
{
}

void KDebuggerFuzzer::run()
{
    QString fuzzmessage = QLatin1String("Fuzzing ");
    fuzzmessage.append(kObjectString(m_object));
    fuzzmessage.append(QLatin1String(" for "));
    fuzzmessage.append(QString::number(m_duration));
    fuzzmessage.append(QLatin1String("ms"));
    emit message(fuzzmessage);
    QElapsedTimer elapsedtimer;
    elapsedtimer.restart();
    while (m_object && elapsedtimer.elapsed() < m_duration) {
        if (m_fuzzevents) {
            // send a random event
            QEvent fuzzevent(fuzzeventtypes[KRandom::randomMax(fuzzeventtypessize)]);
            QString eventmessage = QLatin1String("Sending event - ");
            eventmessage.append(kEventString(&fuzzevent));
            emit message(eventmessage);
            QApplication::sendEvent(m_object.data(), &fuzzevent);
        }

        // TODO: set a random property to random value

        QThread::msleep(200);
    }
    fuzzmessage = QLatin1String("Done fuzzing ");
    fuzzmessage.append(kObjectString(m_object));
    emit message(fuzzmessage);
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
    KVBox* fuzzbox;
    QCheckBox* threadfuzzbox;
    QCheckBox* eventfuzzbox;
    QCheckBox* propertyfuzzbox;
    KTimeEdit* durationfuzzedit;
    KPushButton* fuzzbutton;
    KTextEdit* fuzzedit;
    KThreadPool* fuzzpool;

public Q_SLOTS:
    void slotUpdateObjects();
    void slotItemSelectionChanged();
    void slotItemChanged(QTableWidgetItem *propertyvalueitem);
    void slotFuzzReleased();
    void slotCheckFuzzState();
    void slotMessage(const QString &message);

protected:
    bool eventFilter(QObject *object, QEvent *event) final;

private:
    void addObject(QObject *object, QTreeWidgetItem *parentitem);

    QPointer<QObject> m_object;
    QMap<QObject*,QPointer<QObject>> m_objects;
    QMutex m_eventsmutex;
    QTimer* m_fuzzchecktimer;
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
    propertieswidget(nullptr),
    fuzzbox(nullptr),
    threadfuzzbox(nullptr),
    eventfuzzbox(nullptr),
    propertyfuzzbox(nullptr),
    durationfuzzedit(nullptr),
    fuzzbutton(nullptr),
    fuzzedit(nullptr),
    fuzzpool(nullptr),
    m_fuzzchecktimer(nullptr)
{
    m_fuzzchecktimer = new QTimer(this);
    m_fuzzchecktimer->setInterval(200);
    connect(
        m_fuzzchecktimer, SIGNAL(timeout()),
        this, SLOT(slotCheckFuzzState())
    );
}

void KDebuggerPrivate::addObject(QObject *object, QTreeWidgetItem *parentitem)
{
    QTreeWidgetItem* objectitem = new QTreeWidgetItem(parentitem);
    objectitem->setText(0, kObjectString(object));
    objectitem->setData(0, Qt::UserRole, QVariant::fromValue(object));
    objectitem->setExpanded(true);
    m_objects.insert(object, object);
    foreach (QObject *childobject, object->children()) {
        addObject(childobject, objectitem);
    }
}

void KDebuggerPrivate::slotCheckFuzzState()
{
    if (fuzzpool->activeThreadCount() > 0) {
        if (!m_fuzzchecktimer->isActive()) {
            m_fuzzchecktimer->start();
            objectssearchline->setEnabled(false);
            objectswidget->setEnabled(false);
            propertieswidget->setEnabled(false);
            threadfuzzbox->setEnabled(false);
            eventfuzzbox->setEnabled(false);
            propertyfuzzbox->setEnabled(false);
            durationfuzzedit->setEnabled(false);
            fuzzbutton->setText(i18n("Stop"));
            fuzzbutton->setIcon(KIcon("process-stop"));
        }
    } else {
        objectssearchline->setEnabled(true);
        objectswidget->setEnabled(true);
        propertieswidget->setEnabled(true);
        threadfuzzbox->setEnabled(true);
        eventfuzzbox->setEnabled(true);
        propertyfuzzbox->setEnabled(true);
        durationfuzzedit->setEnabled(true);
        fuzzbutton->setText(i18n("Start"));
        fuzzbutton->setIcon(KIcon("system-run"));
        m_fuzzchecktimer->stop();
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
            eventline.append(kEventString(event));
            QMutexLocker locker(&m_eventsmutex);
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
    m_objects.clear();
    objectswidget->setEnabled(false);
    propertieswidget->setEnabled(false);
    addObject(qApp, objectswidget->invisibleRootItem());
    foreach (QWidget *widget, QApplication::allWidgets()) {
        addObject(widget, objectswidget->invisibleRootItem());
    }
    objectswidget->setEnabled(true);
    propertieswidget->setEnabled(true);
}

void KDebuggerPrivate::slotItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedobjects = objectswidget->selectedItems();
    if (selectedobjects.isEmpty()) {
        eventsedit->clear();
        propertieswidget->clearContents();
        propertieswidget->setRowCount(0);
        return;
    }
    QTreeWidgetItem* objectitem = selectedobjects.first();
    QObject* object = qvariant_cast<QObject*>(objectitem->data(0, Qt::UserRole));
    if (m_object) {
        m_object->removeEventFilter(this);
    }
    m_object = m_objects.value(object);
    eventsedit->clear();
    propertieswidget->clearContents();
    propertieswidget->setRowCount(0);
    if (!m_object) {
        return;
    }
    kDebug() << "watching" << m_object;
    m_object->installEventFilter(this);

    propertieswidget->blockSignals(true);
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
    const int propertyindex = propertyvalueitem->data(Qt::UserRole).toInt();
    const QMetaObject* metaobject = m_object->metaObject();
    QMetaProperty metaproperty = metaobject->property(propertyindex);
    metaproperty.write(m_object, propertyvalueitem->text());
}

void KDebuggerPrivate::slotFuzzReleased()
{
    if (threadfuzzbox->checkState() == Qt::Unchecked) {
        fuzzpool->setMaxThreadCount(1);
    } else {
        fuzzpool->setMaxThreadCount(QThread::idealThreadCount());
    }

    fuzzedit->clear();
    for (int i = 0; i < fuzzpool->maxThreadCount(); i++) {
        const qint64 duration = (QTime(0, 0, 0).secsTo(durationfuzzedit->time()) * 1000);
        const bool fuzzevents = (eventfuzzbox->checkState() != Qt::Unchecked);
        const bool fuzzproperties = (propertyfuzzbox->checkState() != Qt::Unchecked);
        KDebuggerFuzzer* fuzzer = new KDebuggerFuzzer(
            fuzzpool,
            m_object, duration, fuzzevents, fuzzproperties
        );
        connect(
            fuzzer, SIGNAL(message(QString)),
            this, SLOT(slotMessage(QString))
        );
        fuzzpool->start(fuzzer);
    }

    slotCheckFuzzState();
}

void KDebuggerPrivate::slotMessage(const QString &message)
{
    fuzzedit->append(message);
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
    d->propertieswidget->setRowCount(0);
    d->propertieswidget->setColumnCount(2);
    const QStringList tableheaders = QStringList()
        << i18n("Property")
        << i18n("Value");
    d->propertieswidget->setHorizontalHeaderLabels(tableheaders);
    QHeaderView* verticalheader = d->propertieswidget->verticalHeader();
    verticalheader->setVisible(false);
    QHeaderView* horizontalheader = d->propertieswidget->horizontalHeader();
    horizontalheader->setMovable(false);
    horizontalheader->setStretchLastSection(false);
    horizontalheader->setResizeMode(0, QHeaderView::Stretch);
    horizontalheader->setResizeMode(1, QHeaderView::Stretch);
    d->tabwidget->addTab(d->propertieswidget, KIcon("document-properties"), i18n("Properties"));

    d->fuzzbox = new KVBox(d->tabwidget);
    d->threadfuzzbox = new QCheckBox(d->fuzzbox);
    d->threadfuzzbox->setText(i18n("Thread fuzz"));
    d->threadfuzzbox->setChecked(true);
    d->eventfuzzbox = new QCheckBox(d->fuzzbox);
    d->eventfuzzbox->setText(i18n("Event fuzz"));
    d->eventfuzzbox->setChecked(true);
    d->propertyfuzzbox = new QCheckBox(d->fuzzbox);
    d->propertyfuzzbox->setText(i18n("Property fuzz"));
    d->propertyfuzzbox->setChecked(true);
    d->durationfuzzedit = new KTimeEdit(d->fuzzbox);
    d->durationfuzzedit->setMinimumTime(QTime(0, 0, 1));
    d->fuzzbutton = new KPushButton(d->fuzzbox);
    d->fuzzbutton->setText(i18n("Start"));
    d->fuzzbutton->setIcon(KIcon("system-run"));
    d->fuzzedit = new KTextEdit(d->fuzzbox);
    d->fuzzedit->setReadOnly(false);
    d->tabwidget->addTab(d->fuzzbox, KIcon("debug-run"), i18n("Fuzz"));

    d->fuzzpool = new KThreadPool(this);

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
    connect(
        d->fuzzbutton, SIGNAL(released()),
        d, SLOT(slotFuzzReleased())
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