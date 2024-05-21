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

#ifndef KNUMVALIDATOR_H
#define KNUMVALIDATOR_H

#include <kdeui_export.h>

#include <QValidator>

/**
 * @short A locale-aware QIntValidator
 *
 * QIntValidator extends QIntValidator to be locale-aware. That means that - subject to not being
 * disabled - the system locale thousand separator, positive and negative sign are used for
 * validation.
 *
 * @author Ivailo Monev <xakepa10@gmail.com>
 * @see KDoubleValidator
 **/
class KDEUI_EXPORT KIntValidator : public QIntValidator
{
    Q_OBJECT
    Q_PROPERTY(bool acceptLocalizedNumbers READ acceptLocalizedNumbers WRITE setAcceptLocalizedNumbers)
public:
    /**
     * Constuct a locale-aware KIntValidator with default range 
     */
    explicit KIntValidator(QObject *parent);

    /**
     * Constuct a locale-aware KIntValidator for the speicified range and decimals
     */
    KIntValidator(int bottom, int top, QObject *parent);

    virtual ~KIntValidator();

    /** @return whether localized numbers are accepted, enabled by default */
    bool acceptLocalizedNumbers() const;

    /** Sets whether to accept localized numbers, enabled by default */
    void setAcceptLocalizedNumbers(bool accept);

private:
    class KIntValidatorPrivate;
    KIntValidatorPrivate* const d;
};

/**
 * @short A locale-aware QDoubleValidator
 *
 * KDoubleValidator extends QDoubleValidator to be locale-aware. That means that - subject to not
 * being disabled - the system locale decimal point, thousand separator, positive and negative sign
 * are used for validation.
 *
 * @author Ivailo Monev <xakepa10@gmail.com>
 * @see KIntValidator
 **/
class KDEUI_EXPORT KDoubleValidator : public QDoubleValidator
{
    Q_OBJECT
    Q_PROPERTY(bool acceptLocalizedNumbers READ acceptLocalizedNumbers WRITE setAcceptLocalizedNumbers)
public:
    /**
     * Constuct a locale-aware KDoubleValidator with default range 
     */
    explicit KDoubleValidator(QObject *parent);

    /**
     * Constuct a locale-aware KDoubleValidator for the speicified range and decimals
     */
    KDoubleValidator(double bottom, double top, int decimals, QObject *parent);

    virtual ~KDoubleValidator();

    /** @return whether localized numbers are accepted, enabled by default */
    bool acceptLocalizedNumbers() const;

    /** Sets whether to accept localized numbers, enabled by default */
    void setAcceptLocalizedNumbers(bool accept);

private:
    class KDoubleValidatorPrivate;
    KDoubleValidatorPrivate* const d;
};

#endif
