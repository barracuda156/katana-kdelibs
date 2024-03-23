/*
 *   Copyright 2008 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "label.h"

#include <QApplication>
#include <QDir>
#include <QtGui/qgraphicssceneevent.h>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QtGui/qstyleoption.h>

#include <kcolorscheme.h>
#include <kglobalsettings.h>
#include <kmimetype.h>

#include "private/themedwidgetinterface_p.h"
#include "svg.h"
#include "theme.h"

namespace Plasma
{

class LabelPrivate : public ThemedWidgetInterface<Label>
{
public:
    LabelPrivate(Label *label)
        : ThemedWidgetInterface<Label>(label),
          svg(nullptr),
          textSelectable(false),
          hasLinks(false)
    {
    }

    ~LabelPrivate()
    {
        delete svg;
    }

    void setPixmap()
    {
        if (imagePath.isEmpty()) {
            delete svg;
            svg = nullptr;
            return;
        }

        KMimeType::Ptr mime = KMimeType::findByPath(absImagePath);
        QPixmap pm(q->size().toSize());

        if (mime->is("image/svg+xml") || mime->is("image/svg+xml-compressed")) {
            if (!svg || svg->imagePath() != absImagePath) {
                delete svg;
                svg = new Svg();
                svg->setImagePath(imagePath);
                QObject::connect(svg, SIGNAL(repaintNeeded()), q, SLOT(setPixmap()));
            }

            QPainter p(&pm);
            svg->paint(&p, pm.rect());
        } else {
            delete svg;
            svg = nullptr;
            pm = QPixmap(absImagePath);
        }

        q->nativeWidget()->setPixmap(pm);
    }

    QString imagePath;
    QString absImagePath;
    Svg *svg;
    bool textSelectable;
    bool hasLinks;
};

Label::Label(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
      d(new LabelPrivate(this))
{
    QLabel *native = new QLabel;

    native->setWindowFlags(native->windowFlags()|Qt::BypassGraphicsProxyWidget);
    native->setAttribute(Qt::WA_NoSystemBackground);
    native->setWordWrap(true);
    native->setWindowIcon(QIcon());

    connect(native, SIGNAL(linkActivated(QString)), this, SIGNAL(linkActivated(QString)));
    connect(native, SIGNAL(linkHovered(QString)), this, SIGNAL(linkHovered(QString)));

    d->setWidget(native);
    d->initTheming();
}

Label::~Label()
{
    delete d;
}

void Label::setText(const QString &text)
{
    d->hasLinks = text.contains("<a ", Qt::CaseInsensitive);
    nativeWidget()->setText(text);
    updateGeometry();
}

QString Label::text() const
{
    return nativeWidget()->text();
}

void Label::setImage(const QString &path)
{
    if (d->imagePath == path) {
        return;
    }

    delete d->svg;
    d->svg = nullptr;
    d->imagePath = path;

    const bool absolutePath = (!path.isEmpty() && path[0] == '/');
    if (absolutePath) {
        d->absImagePath = path;
    } else {
        d->absImagePath = Theme::defaultTheme()->imagePath(path);
    }

    d->setPixmap();
}

QString Label::image() const
{
    return d->imagePath;
}

void Label::setScaledContents(bool scaled)
{
    nativeWidget()->setScaledContents(scaled);
}

bool Label::hasScaledContents() const
{
    return nativeWidget()->hasScaledContents();
}

void Label::setTextSelectable(bool enable)
{
    if (enable) {
        nativeWidget()->setTextInteractionFlags(Qt::TextBrowserInteraction);
    } else {
        nativeWidget()->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    }

    d->textSelectable = enable;
}

bool Label::textSelectable() const
{
  return d->textSelectable;
}

void Label::setAlignment(Qt::Alignment alignment)
{
    nativeWidget()->setAlignment(alignment);
}

Qt::Alignment Label::alignment() const
{
    return nativeWidget()->alignment();
}

void Label::setWordWrap(bool wrap)
{
    nativeWidget()->setWordWrap(wrap);
}

bool Label::wordWrap() const
{
    return nativeWidget()->wordWrap();
}

void Label::setStyleSheet(const QString &stylesheet)
{
    widget()->setStyleSheet(stylesheet);
}

QString Label::styleSheet()
{
    return widget()->styleSheet();
}

QLabel *Label::nativeWidget() const
{
    return static_cast<QLabel*>(widget());
}

void Label::dataUpdated(const QString &sourceName, const Plasma::DataEngine::Data &data)
{
    Q_UNUSED(sourceName);

    QStringList texts;
    foreach (const QVariant &v, data) {
        if (v.canConvert(QVariant::String)) {
            texts << v.toString();
        }
    }

    setText(texts.join(" "));
}

void Label::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (d->textSelectable || d->hasLinks){
        QContextMenuEvent contextMenuEvent(QContextMenuEvent::Reason(event->reason()),
                                           event->pos().toPoint(), event->screenPos(), event->modifiers());
        QApplication::sendEvent(nativeWidget(), &contextMenuEvent);
    }else{
        event->ignore();
    }
}

void Label::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    d->setPixmap();
    QGraphicsProxyWidget::resizeEvent(event);
}

void Label::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsProxyWidget::mousePressEvent(event);
    //FIXME: when QTextControl accept()s mouse press events (as of Qt 4.6.2, it processes them
    //but never marks them as accepted) the following event->accept() can be removed
    if (d->textSelectable || d->hasLinks) {
        event->accept();
    }
}

void Label::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (d->textSelectable) {
        QGraphicsProxyWidget::mouseMoveEvent(event);
    }
}

void Label::changeEvent(QEvent *event)
{
    d->changeEvent(event);
    QGraphicsProxyWidget::changeEvent(event);
}

bool Label::event(QEvent *event)
{
    d->event(event);
    return QGraphicsProxyWidget::event(event);
}

QVariant Label::itemChange(GraphicsItemChange change, const QVariant & value)
{
    if (change == QGraphicsItem::ItemCursorHasChanged) {
        nativeWidget()->setCursor(cursor());
    }

    return QGraphicsWidget::itemChange(change, value);
}

QSizeF Label::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    if (sizePolicy().verticalPolicy() == QSizePolicy::Fixed) {
        return QGraphicsProxyWidget::sizeHint(Qt::PreferredSize, constraint);
    } else {
        return QGraphicsProxyWidget::sizeHint(which, constraint);
    }
}

} // namespace Plasma

#include "moc_label.cpp"

