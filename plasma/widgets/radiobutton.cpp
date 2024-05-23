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

#include "radiobutton.h"
#include "private/themedwidgetinterface_p.h"
#include "svg.h"

#include <QRadioButton>

namespace Plasma
{

class RadioButtonPrivate : public ThemedWidgetInterface<RadioButton>
{
public:
    RadioButtonPrivate(RadioButton *radio)
        : ThemedWidgetInterface<RadioButton>(radio)
    {
    }
};

RadioButton::RadioButton(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
    d(new RadioButtonPrivate(this))
{
    QRadioButton *native = new QRadioButton();
    connect(native, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));
    d->setWidget(native);
    native->setWindowIcon(QIcon());
    native->setAttribute(Qt::WA_NoSystemBackground);
    d->initTheming();
}

RadioButton::~RadioButton()
{
    delete d;
}

void RadioButton::setText(const QString &text)
{
    nativeWidget()->setText(text);
}

QString RadioButton::text() const
{
    return nativeWidget()->text();
}

QRadioButton *RadioButton::nativeWidget() const
{
    return static_cast<QRadioButton*>(widget());
}

void RadioButton::setChecked(bool checked)
{
    nativeWidget()->setChecked(checked);
}

bool RadioButton::isChecked() const
{
    return nativeWidget()->isChecked();
}

void RadioButton::changeEvent(QEvent *event)
{
    d->changeEvent(event);
    QGraphicsProxyWidget::changeEvent(event);
}

} // namespace Plasma

#include "moc_radiobutton.cpp"

