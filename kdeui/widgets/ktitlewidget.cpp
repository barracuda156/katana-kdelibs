/* This file is part of the KDE libraries
   Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
   Copyright (C) 2007 Michaël Larouche <larouche@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB. If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ktitlewidget.h"
#include "kicon.h"
#include "kiconloader.h"
#include "kpixmapwidget.h"

#include <QFrame>
#include <QLabel>
#include <QLayout>

class KTitleWidget::Private
{
public:
    Private()
        : titleLayout(nullptr),
        pixmapWidget(nullptr),
        textLabel(nullptr)
    {
    }

    void updateTextWidget() const
    {
        QFont f = textLabel->font();
        f.setBold(true);
        textLabel->setFont(f);
    }

    QHBoxLayout *titleLayout;
    KPixmapWidget *pixmapWidget;
    QLabel *textLabel;
};

KTitleWidget::KTitleWidget(QWidget *parent)
  : QWidget(parent),
    d(new Private())
{
    QFrame *titleFrame = new QFrame(this);
    titleFrame->setAutoFillBackground(true);
    titleFrame->setFrameShape(QFrame::StyledPanel);
    titleFrame->setFrameShadow(QFrame::Plain);
    titleFrame->setBackgroundRole(QPalette::Base);

    // default image / text part start
    d->titleLayout = new QHBoxLayout(titleFrame);
    d->titleLayout->setMargin(6);

    d->textLabel = new QLabel(titleFrame);
    d->textLabel->setVisible(false);
    d->textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);

    d->pixmapWidget = new KPixmapWidget(titleFrame);
    d->pixmapWidget->setVisible(false);

    d->titleLayout->addWidget(d->textLabel);
    d->titleLayout->addWidget(d->pixmapWidget);
    d->titleLayout->setStretchFactor(d->textLabel, 1);
    // default image / text part end

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(titleFrame);
    mainLayout->setMargin(0);
    setLayout(mainLayout);
}

KTitleWidget::~KTitleWidget()
{
    delete d;
}

QString KTitleWidget::text() const
{
    return d->textLabel->text();
}

QPixmap KTitleWidget::pixmap() const
{
    return d->pixmapWidget->pixmap();
}

void KTitleWidget::setBuddy(QWidget *buddy)
{
    d->textLabel->setBuddy(buddy);
}

void KTitleWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::FontChange) {
        d->updateTextWidget();
    }
}

void KTitleWidget::setText(const QString &text, Qt::Alignment alignment)
{
    d->textLabel->setVisible(!text.isNull());

    if (!Qt::mightBeRichText(text)) {
        d->updateTextWidget();
    }

    d->textLabel->setText(text);
    d->textLabel->setAlignment(alignment);
    show();
}

void KTitleWidget::setText(const QString &text, MessageType type)
{
    setPixmap(type);
    setText(text);
}

void KTitleWidget::setPixmap(const QPixmap &pixmap, ImageAlignment alignment)
{
    d->pixmapWidget->setVisible(!pixmap.isNull());

    d->titleLayout->removeWidget(d->textLabel);
    d->titleLayout->removeWidget(d->pixmapWidget);

    if (alignment == ImageLeft) {
        // swap the text and image labels around
        d->titleLayout->addWidget(d->pixmapWidget);
        d->titleLayout->addWidget(d->textLabel);
        d->titleLayout->setStretchFactor(d->textLabel, 1);
        d->titleLayout->setStretchFactor(d->pixmapWidget, 0);
    } else {
        d->titleLayout->addWidget(d->textLabel);
        d->titleLayout->addWidget(d->pixmapWidget);
        d->titleLayout->setStretchFactor(d->textLabel, 1);
        d->titleLayout->setStretchFactor(d->pixmapWidget, 0);
    }

    d->pixmapWidget->setPixmap(pixmap);
}

void KTitleWidget::setPixmap(const QString &icon, ImageAlignment alignment)
{
    setPixmap(KIcon(icon), alignment);
}

void KTitleWidget::setPixmap(const QIcon &icon, ImageAlignment alignment)
{
    setPixmap(icon.pixmap(IconSize(KIconLoader::Dialog)), alignment);
}

void KTitleWidget::setPixmap(MessageType type, ImageAlignment alignment)
{
    switch (type) {
        case KTitleWidget::InfoMessage:
            setPixmap(QLatin1String("dialog-information"), alignment);
            break;
        case KTitleWidget::ErrorMessage:
            setPixmap(QLatin1String("dialog-error"), alignment);
            break;
        case KTitleWidget::WarningMessage:
            setPixmap(QLatin1String("dialog-warning"), alignment);
            break;
        case KTitleWidget::PlainMessage:
            setPixmap(KIcon(), alignment);
            break;
    }
}

#include "moc_ktitlewidget.cpp"
