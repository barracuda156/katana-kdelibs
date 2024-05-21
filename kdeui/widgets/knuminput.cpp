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
#include "kglobal.h"
#include "klocale.h"
#include "kdebug.h"

#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QEvent>

// inverse alignment of QLineEdit to put the text next to the arrows and expand text in the other
// direction
static const Qt::Alignment spinBoxAlignment(const QWidget *widget)
{
    if (widget->layoutDirection() == Qt::RightToLeft) {
        return Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter);
}

static void setupSpinBox(QSpinBox *spinbox, const int value)
{
    spinbox->setLocale(KGlobal::locale()->toLocale());
    spinbox->setValue(value);
}

static void setupDoubleSpinBox(QDoubleSpinBox *spinbox, const double value)
{
    spinbox->setLocale(KGlobal::locale()->toLocale());
    spinbox->setValue(value);
}

class KIntNumInputPrivate
{
public:
    KIntNumInputPrivate()
        : slider(nullptr),
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

    QSlider* slider;
    QSpinBox* spinbox;
    KLocalizedString suffix;
};

KIntNumInput::KIntNumInput(QWidget* parent)
    : QWidget(parent),
    d(new KIntNumInputPrivate())
{
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

    setupSpinBox(d->spinbox, d->spinbox->value());

    setRange(INT_MIN, INT_MAX);
    setSingleStep(1);
    setValue(0);
    setAlignment(spinBoxAlignment(this));
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
    d->slider->setMaximum(max);
    d->spinbox->setMaximum(max);
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

void KIntNumInput::changeEvent(QEvent *event)
{
    switch (event->type()) {
        case QEvent::LocaleChange:
        case QEvent::LanguageChange: {
            setupSpinBox(d->spinbox, d->spinbox->value());
            break;
        }
        default: {
            break;
        }
    }
}

class KDoubleNumInputPrivate
{
public:
    KDoubleNumInputPrivate()
        : slider(nullptr),
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
        spinbox->setValue(value);
    }

    QSlider* slider;
    QDoubleSpinBox* spinbox;
    KLocalizedString suffix;
};

KDoubleNumInput::KDoubleNumInput(QWidget *parent)
    : QWidget(parent),
    d(new KDoubleNumInputPrivate())
{
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

    setupDoubleSpinBox(d->spinbox, d->spinbox->value());

    setRange(0.0, 9999.0);
    setSingleStep(0.01);
    setDecimals(2);
    setValue(0.0);
    setAlignment(spinBoxAlignment(this));
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
    d->slider->setMaximum(max);
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

void KDoubleNumInput::changeEvent(QEvent *event)
{
    switch (event->type()) {
        case QEvent::LocaleChange:
        case QEvent::LanguageChange: {
            setupDoubleSpinBox(d->spinbox, d->spinbox->value());
            break;
        }
        default: {
            break;
        }
    }
}

#include "moc_knuminput.cpp"
