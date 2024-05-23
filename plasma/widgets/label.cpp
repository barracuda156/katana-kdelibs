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
#include <QGraphicsSceneContextMenuEvent>
#include <QLabel>
#include <QIcon>

#include "private/themedwidgetinterface_p.h"

namespace Plasma
{

class LabelPrivate : public ThemedWidgetInterface<Label>
{
public:
    LabelPrivate(Label *label)
        : ThemedWidgetInterface<Label>(label)
    {
    }

    void elideText()
    {
        QFontMetricsF fontmetricsf(q->font());
        const qreal dotx3width = fontmetricsf.width(QLatin1String("..."));
        q->nativeWidget()->setText(fontmetricsf.elidedText(originaltext, Qt::ElideRight, q->size().width() - dotx3width));
    }

    QString originaltext;
};

Label::Label(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
    d(new LabelPrivate(this))
{
    QLabel *native = new QLabel();

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
    if (!nativeWidget()->wordWrap()) {
        d->originaltext = text;
        d->elideText();
    } else {
        nativeWidget()->setText(text);
    }
    updateGeometry();
}

QString Label::text() const
{
    return nativeWidget()->text();
}

void Label::setScaledContents(bool scaled)
{
    nativeWidget()->setScaledContents(scaled);
}

bool Label::hasScaledContents() const
{
    return nativeWidget()->hasScaledContents();
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
    if (!wrap) {
        d->originaltext = nativeWidget()->text();
        d->elideText();
    } else {
        nativeWidget()->setText(d->originaltext);
        d->originaltext.clear();
    }
}

bool Label::wordWrap() const
{
    return nativeWidget()->wordWrap();
}

QLabel *Label::nativeWidget() const
{
    return static_cast<QLabel*>(widget());
}

void Label::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    const Qt::TextInteractionFlags textFlags = nativeWidget()->textInteractionFlags();
    if (textFlags & Qt::TextSelectableByMouse || textFlags & Qt::LinksAccessibleByMouse){
        QContextMenuEvent contextMenuEvent(
            QContextMenuEvent::Reason(event->reason()),
            event->pos().toPoint(), event->screenPos(), event->modifiers()
        );
        QApplication::sendEvent(nativeWidget(), &contextMenuEvent);
    } else{
        event->ignore();
    }
}

void Label::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    QGraphicsProxyWidget::resizeEvent(event);
    if (!nativeWidget()->wordWrap()) {
        d->elideText();
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
    return QGraphicsProxyWidget::itemChange(change, value);
}

QSizeF Label::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    if (sizePolicy().verticalPolicy() == QSizePolicy::Fixed) {
        return QGraphicsProxyWidget::sizeHint(Qt::PreferredSize, constraint);
    }
    return QGraphicsProxyWidget::sizeHint(which, constraint);
}

} // namespace Plasma

#include "moc_label.cpp"

