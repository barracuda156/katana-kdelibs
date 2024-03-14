/**********************************************************************
**
**
** KIntValidator:
**   Copyright (C) 1999 Glen Parker <glenebob@nwlink.com>
** KDoubleValidator:
**   Copyright (c) 2002 Marc Mutz <mutz@kde.org>
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
**
** You should have received a copy of the GNU Library General Public
** License along with this library; if not, write to the Free
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
*****************************************************************************/

#include "knumvalidator.h"

#include <QWidget>
#include <QString>

#include <klocale.h>
#include <kglobal.h>
#include <kdebug.h>

///////////////////////////////////////////////////////////////
//  Implementation of KIntValidator
//
class KIntValidator::KIntValidatorPrivate
{
public:
    KIntValidatorPrivate()
     : _base(0), _min(0), _max(0)
    {
    }

    int _base;
    int _min;
    int _max;
};

KIntValidator::KIntValidator ( QWidget *parent, int base)
    : QValidator(parent),
    d(new KIntValidatorPrivate())
{
    setBase(base);
}

KIntValidator::KIntValidator(int bottom, int top, QWidget *parent, int base)
    : QValidator(parent),
    d(new KIntValidatorPrivate())
{
    setBase(base);
    setRange(bottom, top);
}

KIntValidator::~KIntValidator()
{
    delete d;
}

QValidator::State KIntValidator::validate(QString &str, int &) const
{
    bool ok = false;
    int val = 0;
    QString newStr;

    newStr = str.trimmed();
    if (d->_base > 10) {
        newStr = newStr.toUpper();
    }

    if (newStr == QLatin1String("-")) {
        // a special case
        if ((d->_min || d->_max) && d->_min >= 0) {
            ok = false;
        } else {
            return QValidator::Acceptable;
        }
    } else if (!newStr.isEmpty()) {
        val = newStr.toInt(&ok, d->_base);
    } else {
        val = 0;
        ok = true;
    }

    if (!ok) {
        return QValidator::Invalid;
    }

    if ((!d->_min && ! d->_max) || (val >= d->_min && val <= d->_max)) {
        return QValidator::Acceptable;
    }

    if (d->_max && d->_min >= 0 && val < 0) {
        return QValidator::Invalid;
    }

    return QValidator::Intermediate;
}

void KIntValidator::fixup(QString &str) const
{
    int dummy = 0;
    int val = 0;
    QValidator::State state;

    state = validate(str, dummy);

    if (state == QValidator::Invalid || state == QValidator::Acceptable) {
        return;
    }

    if (!d->_min && !d->_max) {
        return;
    }

    val = str.toInt(0, d->_base);

    if (val < d->_min) {
        val = d->_min;
    }
    if (val > d->_max) {
        val = d->_max;
    }

    str.setNum(val, d->_base);
}

void KIntValidator::setRange(int bottom, int top)
{
    d->_min = bottom;
    d->_max = top;

    if (d->_max < d->_min) {
        d->_max = d->_min;
    }
}

void KIntValidator::setBase(int base)
{
    d->_base = base;
    if (d->_base < 2) {
        d->_base = 2;
    }
    if (d->_base > 36) {
        d->_base = 36;
    }
}

int KIntValidator::bottom() const
{
    return d->_min;
}

int KIntValidator::top() const
{
    return d->_max;
}

int KIntValidator::base() const
{
    return d->_base;
}


///////////////////////////////////////////////////////////////
//  Implementation of KDoubleValidator
//

static void kAcceptLocalizedNumbers(KDoubleValidator *validator, const bool accept)
{
    if (accept) {
        validator->setLocale(KGlobal::locale()->toLocale());
    } else {
        validator->setLocale(QLocale::c());
    }
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
