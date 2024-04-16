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

#include "kmessagewidget.h"
#include "klocale.h"
#include "kaction.h"
#include "kicon.h"
#include "kiconloader.h"
#include "kcolorscheme.h"
#include "kpixmapwidget.h"
#include "kdebug.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QPen>

static const qreal s_roundness = 4;
static const qreal s_bordersize = 0.6;
static const qreal s_margin = 4;

class KMessageLabel : public QLabel
{
    Q_OBJECT
public:
    KMessageLabel(QWidget *parent);

    QColor bg;
    QColor border;

protected:
    void paintEvent(QPaintEvent *event) final;
};

KMessageLabel::KMessageLabel(QWidget *parent)
    : QLabel(parent)
{
    setContentsMargins(s_margin, s_margin, s_margin, s_margin);
}

void KMessageLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen borderpen(border);
    borderpen.setWidth(s_bordersize * 2);
    painter.setPen(borderpen);
    QRectF widgetrect = rect();
    painter.drawRoundedRect(widgetrect, s_roundness, s_roundness, Qt::AbsoluteSize);
    painter.setBrush(bg);
    widgetrect = widgetrect.adjusted(s_bordersize, s_bordersize, -s_bordersize, -s_bordersize);
    painter.drawRoundedRect(widgetrect, s_roundness, s_roundness, Qt::AbsoluteSize);
    QLabel::paintEvent(event);
}

//---------------------------------------------------------------------
// KMessageWidgetPrivate
//---------------------------------------------------------------------
class KMessageWidgetPrivate
{
public:
    KMessageWidgetPrivate();
    ~KMessageWidgetPrivate();

    void updateColors();

    QVBoxLayout* mainlayout;
    QHBoxLayout* messagelayout;
    KPixmapWidget* iconwidget;
    KMessageLabel* textlabel;
    QToolButton* closebutton;
    QIcon icon;
    KMessageWidget::MessageType messagetype;
    QHBoxLayout* buttonslayout;
    QList<QToolButton*> buttons;
};

KMessageWidgetPrivate::KMessageWidgetPrivate()
    : mainlayout(nullptr),
    messagelayout(nullptr),
    iconwidget(nullptr),
    textlabel(nullptr),
    closebutton(nullptr),
    buttonslayout(nullptr),
    messagetype(KMessageWidget::Information)
{
}

KMessageWidgetPrivate::~KMessageWidgetPrivate()
{
    qDeleteAll(buttons);
    buttons.clear();
    delete buttonslayout;
    delete messagelayout;
}

void KMessageWidgetPrivate::updateColors()
{
    const KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    switch (messagetype) {
        case KMessageWidget::Information: {
            // even tho the selection color may be more suitable for that it cannot be used because
            // the text is selectable
            textlabel->bg = scheme.background(KColorScheme::PositiveBackground).color();
            break;
        }
        case KMessageWidget::Warning: {
            textlabel->bg = scheme.background(KColorScheme::NeutralBackground).color();
            break;
        }
        case KMessageWidget::Error: {
            textlabel->bg = scheme.background(KColorScheme::NegativeBackground).color();
            break;
        }
    }
    textlabel->border = KColorScheme::shade(textlabel->bg, KColorScheme::DarkShade);
}

//---------------------------------------------------------------------
// KMessageWidget
//---------------------------------------------------------------------
KMessageWidget::KMessageWidget(QWidget *parent)
    : QWidget(parent),
    d(new KMessageWidgetPrivate())
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    d->mainlayout = new QVBoxLayout(this);
    setLayout(d->mainlayout);

    d->messagelayout = new QHBoxLayout();

    d->iconwidget = new KPixmapWidget(this);
    d->iconwidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d->iconwidget->hide();
    d->messagelayout->addWidget(d->iconwidget);

    d->textlabel = new KMessageLabel(this);
    d->textlabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    d->textlabel->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::LinksAccessibleByMouse);
    d->textlabel->setAlignment(Qt::AlignCenter);
    connect(d->textlabel, SIGNAL(linkActivated(QString)), this, SIGNAL(linkActivated(QString)));
    connect(d->textlabel, SIGNAL(linkHovered(QString)), this, SIGNAL(linkHovered(QString)));
    d->messagelayout->addWidget(d->textlabel);
    d->mainlayout->addLayout(d->messagelayout);

    d->buttonslayout = new QHBoxLayout();
    d->mainlayout->addLayout(d->buttonslayout);

    // NOTE: the standard close action tooltip refers to document, this is not one
    d->closebutton = new QToolButton(this);
    d->closebutton->setText(i18n("&Close"));
    d->closebutton->setIcon(KIcon("window-close"));
    d->closebutton->setToolTip(i18n("Close message"));
    d->closebutton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(d->closebutton, SIGNAL(clicked()), this, SLOT(animatedHide()));
    d->buttonslayout->addStretch();
    d->buttonslayout->addWidget(d->closebutton, 1, Qt::AlignCenter);
    d->buttonslayout->addStretch();

    d->updateColors();
}

KMessageWidget::~KMessageWidget()
{
    delete d;
}

QString KMessageWidget::text() const
{
    return d->textlabel->text();
}

void KMessageWidget::setText(const QString& text)
{
    d->textlabel->setText(text);
}

KMessageWidget::MessageType KMessageWidget::messageType() const
{
    return d->messagetype;
}

void KMessageWidget::setMessageType(KMessageWidget::MessageType type)
{
    d->messagetype = type;
    d->updateColors();
    update();
}

bool KMessageWidget::wordWrap() const
{
    return d->textlabel->wordWrap();
}

void KMessageWidget::setWordWrap(bool wordWrap)
{
    d->textlabel->setWordWrap(wordWrap);
}

bool KMessageWidget::isCloseButtonVisible() const
{
    return d->closebutton->isVisible();
}

void KMessageWidget::setCloseButtonVisible(bool show)
{
    d->closebutton->setVisible(show);
}

void KMessageWidget::animatedShow()
{
    if (isVisible()) {
        return;
    }

    // yep, no animation. changing the geometry for 500ms looks exactly the same as showing the
    // widget without doing so
    QWidget::show();
}

void KMessageWidget::animatedHide()
{
    if (!isVisible()) {
        return;
    }

    QWidget::hide();
}

QIcon KMessageWidget::icon() const
{
    return d->icon;
}

void KMessageWidget::setIcon(const QIcon& icon)
{
    d->icon = icon;
    if (d->icon.isNull()) {
        d->iconwidget->hide();
    } else {
        const int size = KIconLoader::global()->currentSize(KIconLoader::MainToolbar);
        d->iconwidget->setPixmap(d->icon.pixmap(size));
        d->iconwidget->show();
    }
}

bool KMessageWidget::event(QEvent *event)
{
    const bool result = QWidget::event(event);
    switch (event->type()) {
        case QEvent::PaletteChange: {
            d->updateColors();
            update();
            break;
        }
        case QEvent::ActionChanged:
        case QEvent::ActionAdded:
        case QEvent::ActionRemoved: {
            qDeleteAll(d->buttons);
            d->buttons.clear();
            delete d->buttonslayout;
            d->buttonslayout = new QHBoxLayout();
            d->mainlayout->addLayout(d->buttonslayout);
            d->buttonslayout->addStretch();
            foreach (QAction* action, actions()) {
                QToolButton* button = new QToolButton(this);
                button->setDefaultAction(action);
                button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                d->buttons.append(button);
                d->buttonslayout->addWidget(button, 1, Qt::AlignCenter);
            }
            d->buttonslayout->addWidget(d->closebutton, 1, Qt::AlignCenter);
            d->buttonslayout->addStretch();
            break;
        }
    }
    return result;
}

#include "moc_kmessagewidget.cpp"
#include "kmessagewidget.moc"
