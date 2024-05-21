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

#include "knuminput.h"
#include "knumvalidator.h"
#include "kdebug.h"

#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>

class KIntNumInputPrivate
{
public:
    KIntNumInputPrivate()
        : validator(nullptr),
        slider(nullptr),
        spinbox(nullptr)
    {
    }

    void _k_valueChanged(int value)
    {
        if (suffix.isEmpty()) {
            spinbox->setSuffix(QString());
        } else {
            spinbox->setSuffix(suffix.subs(value).toString());
        }
        slider->setValue(value);
    }

    KIntValidator* validator;
    QSlider* slider;
    QSpinBox* spinbox;
    KLocalizedString suffix;
};

KIntNumInput::KIntNumInput(QWidget* parent)
    : QWidget(parent),
    d(new KIntNumInputPrivate())
{
    d->validator = new KIntValidator(this);
    QHBoxLayout* hboxlayout = new QHBoxLayout(this);
    hboxlayout->setMargin(0);
    d->slider = new QSlider(Qt::Horizontal, this);
    d->slider->setVisible(false);
    connect(
        d->slider, SIGNAL(sliderMoved(int)),
        this, SLOT(setValue(int))
    );
    hboxlayout->addWidget(d->slider);
    d->spinbox = new QSpinBox(this);
    connect(
        d->spinbox, SIGNAL(valueChanged(int)),
        this, SIGNAL(valueChanged(int))
    );
    connect(
        d->spinbox, SIGNAL(valueChanged(int)),
        this, SLOT(_k_valueChanged(int))
    );
    connect(
        d->spinbox, SIGNAL(editingFinished()),
        this, SIGNAL(editingFinished())
    );
    hboxlayout->addWidget(d->spinbox);
    setLayout(hboxlayout);

    setFocusProxy(d->spinbox);
    setRange(INT_MIN, INT_MAX);
    setSingleStep(1);
    setBase(10);
    setValue(0);
}

KIntNumInput::~KIntNumInput()
{
    delete d;
}

void KIntNumInput::setRange(int min, int max)
{
    d->slider->setRange(min, max);
    d->spinbox->setRange(min, max);
}

int KIntNumInput::value() const
{
    return d->spinbox->value();
}

int KIntNumInput::minimum() const
{
    return d->spinbox->minimum();
}

void KIntNumInput::setMinimum(int min)
{
    d->slider->setMinimum(min);
    d->spinbox->setMinimum(min);
}

int KIntNumInput::maximum() const
{
    return d->spinbox->maximum();
}

void KIntNumInput::setMaximum(int max)
{
    d->spinbox->setMaximum(max);
    d->slider->setMaximum(max);
}

int KIntNumInput::singleStep() const
{
    return d->spinbox->singleStep();
}

void KIntNumInput::setSingleStep(int singleStep)
{
    d->slider->setSingleStep(singleStep);
    d->slider->setPageStep(singleStep);
    d->spinbox->setSingleStep(singleStep);
}

QString KIntNumInput::suffix() const
{
    return d->spinbox->suffix();
}

QString KIntNumInput::prefix() const
{
    return d->spinbox->prefix();
}

QString KIntNumInput::specialValueText() const
{
    return d->spinbox->specialValueText();
}

void KIntNumInput::setSpecialValueText(const QString &text)
{
    d->spinbox->setSpecialValueText(text);
}

Qt::Alignment KIntNumInput::alignment() const
{
    return d->spinbox->alignment();
}

void KIntNumInput::setAlignment(const Qt::Alignment alignment)
{
    d->spinbox->setAlignment(alignment);
}

int KIntNumInput::base() const
{
    return d->validator->base();
}

void KIntNumInput::setBase(int base)
{
    d->validator->setBase(base);
}

bool KIntNumInput::sliderEnabled() const
{
    return d->slider->isVisible();
}

void KIntNumInput::setSliderEnabled(bool enabled)
{
    d->slider->setVisible(enabled);
}

void KIntNumInput::setSteps(int single, int page)
{
    d->slider->setSingleStep(single);
    d->slider->setPageStep(page);
}

QValidator::State KIntNumInput::validate(QString &input, int &pos) const
{
    return d->validator->validate(input, pos);
}

void KIntNumInput::fixup(QString &input) const
{
    d->validator->fixup(input);
}

void KIntNumInput::setValue(int value)
{
    d->spinbox->setValue(value);
}

void KIntNumInput::setSuffix(const KLocalizedString &suffix)
{
    d->suffix = suffix;
    d->_k_valueChanged(d->spinbox->value());
}

void KIntNumInput::setSuffix(const QString &suffix)
{
    d->suffix = KLocalizedString();
    d->spinbox->setSuffix(suffix);
}

void KIntNumInput::setPrefix(const QString &prefix)
{
    d->spinbox->setPrefix(prefix);
}


class KDoubleNumInputPrivate
{
public:
    KDoubleNumInputPrivate()
        : validator(nullptr),
        slider(nullptr),
        spinbox(nullptr)
    {
    }

    void _k_valueChanged(double value)
    {
        if (suffix.isEmpty()) {
            spinbox->setSuffix(QString());
        } else {
            spinbox->setSuffix(suffix.subs(value).toString());
        }
        slider->setValue(qRound(value));
    }

    void _k_sliderMoved(int value)
    {
        spinbox->setValue(qRound(value));
    }

    KDoubleValidator* validator;
    QSlider* slider;
    QDoubleSpinBox* spinbox;
    KLocalizedString suffix;
};

KDoubleNumInput::KDoubleNumInput(QWidget *parent)
    : QWidget(parent),
    d(new KDoubleNumInputPrivate())
{
    d->validator = new KDoubleValidator(this);
    QHBoxLayout* hboxlayout = new QHBoxLayout(this);
    hboxlayout->setMargin(0);
    d->slider = new QSlider(Qt::Horizontal, this);
    d->slider->setVisible(false);
    connect(
        d->slider, SIGNAL(sliderMoved(int)),
        this, SLOT(_k_sliderMoved(int))
    );
    hboxlayout->addWidget(d->slider);
    d->spinbox = new QDoubleSpinBox(this);
    connect(
        d->spinbox, SIGNAL(valueChanged(double)),
        this, SIGNAL(valueChanged(double))
    );
    connect(
        d->spinbox, SIGNAL(valueChanged(double)),
        this, SLOT(_k_valueChanged(double))
    );
    connect(
        d->spinbox, SIGNAL(editingFinished()),
        this, SIGNAL(editingFinished())
    );
    hboxlayout->addWidget(d->spinbox);
    setLayout(hboxlayout);

    setFocusProxy(d->spinbox);
    setRange(0.0, 9999.0);
    setSingleStep(0.01);
    setDecimals(2);
    setValue(0.0);
}

KDoubleNumInput::~KDoubleNumInput()
{
    delete d;
}

void KDoubleNumInput::setRange(double min, double max)
{
    d->slider->setRange(qRound(min), qRound(max));
    d->spinbox->setRange(min, max);
}

double KDoubleNumInput::value() const
{
    return d->spinbox->value();
}

double KDoubleNumInput::minimum() const
{
    return d->spinbox->minimum();
}

void KDoubleNumInput::setMinimum(double min)
{
    d->slider->setMinimum(min);
    d->spinbox->setMinimum(min);
}

double KDoubleNumInput::maximum() const
{
    return d->spinbox->maximum();
}

void KDoubleNumInput::setMaximum(double max)
{
    d->spinbox->setMaximum(max);
    d->spinbox->setMaximum(max);
}

double KDoubleNumInput::singleStep() const
{
    return d->spinbox->singleStep();
}

void KDoubleNumInput::setSingleStep(double singleStep)
{
    const int intstep = qRound(singleStep);
    d->slider->setSingleStep(intstep);
    d->slider->setPageStep(intstep);
    d->spinbox->setSingleStep(singleStep);
}

QString KDoubleNumInput::suffix() const
{
    return d->spinbox->suffix();
}

QString KDoubleNumInput::prefix() const
{
    return d->spinbox->prefix();
}

QString KDoubleNumInput::specialValueText() const
{
    return d->spinbox->specialValueText();
}

void KDoubleNumInput::setSpecialValueText(const QString &text)
{
    d->spinbox->setSpecialValueText(text);
}

Qt::Alignment KDoubleNumInput::alignment() const
{
    return d->spinbox->alignment();
}

void KDoubleNumInput::setAlignment(const Qt::Alignment alignment)
{
    d->spinbox->setAlignment(alignment);
}

int KDoubleNumInput::decimals() const
{
    return d->spinbox->decimals();
}

void KDoubleNumInput::setDecimals(int decimals)
{
    d->spinbox->setDecimals(decimals);
}

bool KDoubleNumInput::sliderEnabled() const
{
    return d->slider->isVisible();
}

void KDoubleNumInput::setSliderEnabled(bool enabled)
{
    d->slider->setVisible(enabled);
}

void KDoubleNumInput::setSteps(int single, int page)
{
    d->slider->setSingleStep(single);
    d->slider->setPageStep(page);
}

QValidator::State KDoubleNumInput::validate(QString &input, int &pos) const
{
    return d->validator->validate(input, pos);
}

void KDoubleNumInput::fixup(QString &input) const
{
    d->validator->fixup(input);
}

void KDoubleNumInput::setValue(double value)
{
    d->spinbox->setValue(value);
}

void KDoubleNumInput::setSuffix(const KLocalizedString &suffix)
{
    d->suffix = suffix;
    d->_k_valueChanged(d->spinbox->value());
}

void KDoubleNumInput::setSuffix(const QString &suffix)
{
    d->suffix = KLocalizedString();
    d->spinbox->setSuffix(suffix);
}

void KDoubleNumInput::setPrefix(const QString &prefix)
{
    d->spinbox->setPrefix(prefix);
}

#include "moc_knuminput.cpp"
