/*
 *   Copyright 2024 Ivailo Monev <xakepa10@gmail.com>
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

#include "listwidget.h"
#include "private/style_p.h"

#include <QListWidget>
#include <QScrollBar>

namespace Plasma
{

class ListWidgetPrivate
{
public:
    Plasma::Style::Ptr style;
};

ListWidget::ListWidget(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
    d(new ListWidgetPrivate())
{
    QListWidget *native = new QListWidget();
    setWidget(native);
    native->setWindowIcon(QIcon());
    native->setAttribute(Qt::WA_NoSystemBackground);
    native->setFrameStyle(QFrame::NoFrame);

    d->style = Plasma::Style::sharedStyle();
    native->verticalScrollBar()->setStyle(d->style.data());
    native->horizontalScrollBar()->setStyle(d->style.data());
}

ListWidget::~ListWidget()
{
    delete d;
    Plasma::Style::doneWithSharedStyle();
}

QListWidget* ListWidget::nativeWidget() const
{
    return static_cast<QListWidget*>(widget());
}

}

#include "moc_listwidget.cpp"

