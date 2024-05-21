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

#include "knumvalidator.h"

#include <klocale.h>
#include <kglobal.h>
#include <kdebug.h>

static void kAcceptLocalizedNumbers(QValidator *validator, const bool accept)
{
    if (accept) {
        validator->setLocale(KGlobal::locale()->toLocale());
    } else {
        validator->setLocale(QLocale::c());
    }
}

class KIntValidator::KIntValidatorPrivate
{
public:
    KIntValidatorPrivate()
        : acceptLocalizedNumbers(true)
    {
    }

    bool acceptLocalizedNumbers;
};

KIntValidator::KIntValidator(QObject *parent)
    : QIntValidator(parent),
    d(new KIntValidatorPrivate())
{
    kAcceptLocalizedNumbers(this, true);
}

KIntValidator::KIntValidator(int bottom, int top, QObject *parent)
    : QIntValidator(bottom, top, parent),
    d(new KIntValidatorPrivate())
{
    kAcceptLocalizedNumbers(this, true);
}

KIntValidator::~KIntValidator()
{
    delete d;
}

bool KIntValidator::acceptLocalizedNumbers() const
{
    return d->acceptLocalizedNumbers;
}

void KIntValidator::setAcceptLocalizedNumbers(bool accept)
{
    d->acceptLocalizedNumbers = accept;
    kAcceptLocalizedNumbers(this, accept);
}



class KDoubleValidator::KDoubleValidatorPrivate
{
public:
    KDoubleValidatorPrivate()
        : acceptLocalizedNumbers(true)
    {
    }

    bool acceptLocalizedNumbers;
};

KDoubleValidator::KDoubleValidator(QObject *parent)
    : QDoubleValidator(parent),
    d(new KDoubleValidatorPrivate())
{
    kAcceptLocalizedNumbers(this, true);
}

KDoubleValidator::KDoubleValidator(double bottom, double top, int decimals, QObject *parent)
    : QDoubleValidator(bottom, top, decimals, parent),
    d(new KDoubleValidatorPrivate())
{
    kAcceptLocalizedNumbers(this, true);
}

KDoubleValidator::~KDoubleValidator()
{
    delete d;
}

bool KDoubleValidator::acceptLocalizedNumbers() const
{
    return d->acceptLocalizedNumbers;
}

void KDoubleValidator::setAcceptLocalizedNumbers(bool accept)
{
    d->acceptLocalizedNumbers = accept;
    kAcceptLocalizedNumbers(this, accept);
}

#include "moc_knumvalidator.cpp"
