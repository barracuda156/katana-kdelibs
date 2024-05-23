/*
 *   Copyright 2008 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2008 Marco Martin <notmart@gmail.com>
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

#include "pushbutton.h"
#include "private/actionwidgetinterface_p.h"
#include "private/themedwidgetinterface_p.h"

#include <kpushbutton.h>

namespace Plasma
{

class PushButtonPrivate : public ActionWidgetInterface<PushButton>
{
public:
    PushButtonPrivate(PushButton *pushButton)
        : ActionWidgetInterface<PushButton>(pushButton)
    {
    }
};


PushButton::PushButton(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
      d(new PushButtonPrivate(this))
{
    KPushButton *native = new KPushButton();
    connect(native, SIGNAL(pressed()), this, SIGNAL(pressed()));
    connect(native, SIGNAL(released()), this, SIGNAL(released()));
    connect(native, SIGNAL(clicked()), this, SIGNAL(clicked()));
    connect(native, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));
    setWidget(native);
    native->setAttribute(Qt::WA_NoSystemBackground);
    native->setWindowIcon(QIcon());

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setAcceptHoverEvents(true);

    d->initTheming();
}

PushButton::~PushButton()
{
    delete d;
}

void PushButton::setText(const QString &text)
{
    nativeWidget()->setText(text);
    updateGeometry();
}

QString PushButton::text() const
{
    return nativeWidget()->text();
}

void PushButton::setAction(QAction *action)
{
    d->setAction(action);
}

QAction *PushButton::action() const
{
    return d->action;
}

void PushButton::setIcon(const KIcon &icon)
{
    nativeWidget()->setIcon(icon);
}

void PushButton::setIcon(const QIcon &icon)
{
    setIcon(KIcon(icon));
}

QIcon PushButton::icon() const
{
    return nativeWidget()->icon();
}

void PushButton::setCheckable(bool checkable)
{
    nativeWidget()->setCheckable(checkable);
}

bool PushButton::isCheckable() const
{
    return nativeWidget()->isCheckable();
}

void PushButton::setChecked(bool checked)
{
    nativeWidget()->setChecked(checked);
}

void PushButton::click()
{
    nativeWidget()->animateClick();
}

bool PushButton::isChecked() const
{
    return nativeWidget()->isChecked();
}

bool PushButton::isDown() const
{
    return nativeWidget()->isDown();
}

KPushButton *PushButton::nativeWidget() const
{
    return static_cast<KPushButton*>(widget());
}

void PushButton::changeEvent(QEvent *event)
{
    d->changeEvent(event);
    QGraphicsProxyWidget::changeEvent(event);
}

} // namespace Plasma

#include "moc_pushbutton.cpp"

