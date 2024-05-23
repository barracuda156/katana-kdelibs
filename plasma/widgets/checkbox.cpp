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

#include "checkbox.h"

#include <QCheckBox>

#include "private/themedwidgetinterface_p.h"

namespace Plasma
{

class CheckBoxPrivate : public ThemedWidgetInterface<CheckBox>
{
public:
    CheckBoxPrivate(CheckBox *c)
        : ThemedWidgetInterface<CheckBox>(c)
    {
    }
};

CheckBox::CheckBox(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
      d(new CheckBoxPrivate(this))
{
    QCheckBox *native = new QCheckBox();
    connect(native, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));
    d->setWidget(native);
    native->setWindowIcon(QIcon());
    native->setAttribute(Qt::WA_NoSystemBackground);

    d->initTheming();
}

CheckBox::~CheckBox()
{
    delete d;
}

void CheckBox::setText(const QString &text)
{
    nativeWidget()->setText(text);
}

QString CheckBox::text() const
{
    return nativeWidget()->text();
}

QCheckBox *CheckBox::nativeWidget() const
{
    return static_cast<QCheckBox*>(widget());
}

void CheckBox::setChecked(bool checked)
{
    nativeWidget()->setChecked(checked);
}

bool CheckBox::isChecked() const
{
    return nativeWidget()->isChecked();
}

void CheckBox::changeEvent(QEvent *event)
{
    d->changeEvent(event);
    QGraphicsProxyWidget::changeEvent(event);
}

} // namespace Plasma

#include "moc_checkbox.cpp"

