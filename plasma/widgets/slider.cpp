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

#include "slider.h"
#include "private/style_p.h"
#include "private/themedwidgetinterface_p.h"

#include <QApplication>
#include <QSlider>
#include <QGraphicsSceneWheelEvent>

namespace Plasma
{

class SliderPrivate : public ThemedWidgetInterface<Slider>
{
public:
    SliderPrivate(Slider *slider)
        : ThemedWidgetInterface<Slider>(slider)
    {
    }

    Plasma::Style::Ptr style;
};

Slider::Slider(QGraphicsWidget *parent)
    : QGraphicsProxyWidget(parent),
    d(new SliderPrivate(this))
{
    QSlider *native = new QSlider();

    connect(native, SIGNAL(sliderMoved(int)), this, SIGNAL(sliderMoved(int)));
    connect(native, SIGNAL(valueChanged(int)), this, SIGNAL(valueChanged(int)));

    setWidget(native);
    native->setWindowIcon(QIcon());
    native->setAttribute(Qt::WA_NoSystemBackground);

    d->style = Plasma::Style::sharedStyle();
    native->setStyle(d->style.data());

    d->initTheming();
}

Slider::~Slider()
{
    delete d;
    Plasma::Style::doneWithSharedStyle();
}

void Slider::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    QWheelEvent e(event->pos().toPoint(), event->delta(),event->buttons(),event->modifiers(),event->orientation());
    QApplication::sendEvent(widget(), &e);
    event->accept();
}

void Slider::setMaximum(int max)
{
    nativeWidget()->setMaximum(max);
}

int Slider::maximum() const
{
    return nativeWidget()->maximum();
}

void Slider::setMinimum(int min)
{
    nativeWidget()->setMinimum(min);
}

int Slider::minimum() const
{
    return nativeWidget()->minimum();
}

void Slider::setRange(int min, int max)
{
    nativeWidget()->setRange(min, max);
}

void Slider::setValue(int value)
{
    nativeWidget()->setValue(value);
}

int Slider::value() const
{
    return nativeWidget()->value();
}

void Slider::setOrientation(Qt::Orientation orientation)
{
    nativeWidget()->setOrientation(orientation);
}

Qt::Orientation Slider::orientation() const
{
    return nativeWidget()->orientation();
}

QSlider *Slider::nativeWidget() const
{
    return static_cast<QSlider*>(widget());
}

} // namespace Plasma

#include "moc_slider.cpp"

