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

#ifndef K_NUMINPUT_H
#define K_NUMINPUT_H

#include <kdeui_export.h>
#include <klocalizedstring.h>

#include <QWidget>

class KIntNumInputPrivate;
class KDoubleNumInputPrivate;


/**
 * @short An input control for numbers with base 10.
 *
 * KIntNumInput combines a QSpinBox and optionally a QSlider to make an easy to use control for
 * setting some integer parameter.
 *
 * @see KDoubleNumInput
 */
class KDEUI_EXPORT KIntNumInput : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString specialValueText READ specialValueText WRITE setSpecialValueText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool sliderEnabled READ sliderEnabled WRITE setSliderEnabled)
public:
    /**
     * Constructs an input control for integer values with initial value 0.
     */
    explicit KIntNumInput(QWidget *parent = nullptr);

    virtual ~KIntNumInput();

    /**
     * Spin box and slider proxies
     */
    void setRange(int min, int max);
    int value() const;
    int minimum() const;
    void setMinimum(int min);
    int maximum() const;
    void setMaximum(int max);
    int singleStep() const;
    void setSingleStep(int singleStep);
    QString suffix() const;
    QString prefix() const;
    QString specialValueText() const;
    void setSpecialValueText(const QString &text);
    Qt::Alignment alignment() const;
    void setAlignment(const Qt::Alignment alignment);

    /**
     * @return if slider is enabled.
     * @default disabled
     * @see setSliderEnabled()
     */
    bool sliderEnabled() const;

    /**
     * @param enabled Show the slider
     */
    void setSliderEnabled(bool enabled);

    /**
     * Sets the steps for the slider.
     *
     * @param single The single slider step.
     * @param page The page slider step.
     */
    void setSteps(int single, int page);

public Q_SLOTS:
    /**
     * Spin box and slider proxies
     */
    void setValue(int value);
    void setSuffix(const KLocalizedString &suffix);
    void setSuffix(const QString &suffix);
    void setPrefix(const QString &prefix);

Q_SIGNALS:
    void valueChanged(int);
    void editingFinished();

protected:
    // QWidget reimplementation
    virtual void changeEvent(QEvent *event);

private:
    friend KIntNumInputPrivate;
    KIntNumInputPrivate* const d;
    
    Q_PRIVATE_SLOT(d, void _k_valueChanged(int value));

    Q_DISABLE_COPY(KIntNumInput)
};

/**
 * @short An input control for double numbers.
 *
 * KDoubleNumInput combines a QDoubleSpinBox and optionally a QSlider to make an easy to use
 * control for setting some double parameter.
 *
 * @see KIntNumInput
 */
class KDEUI_EXPORT KDoubleNumInput : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString specialValueText READ specialValueText WRITE setSpecialValueText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(bool sliderEnabled READ sliderEnabled WRITE setSliderEnabled)
public:
    /**
     * Constructs an input control for double values with initial value 0.00.
     */
    explicit KDoubleNumInput(QWidget *parent = nullptr);

    virtual ~KDoubleNumInput();

    /**
     * Spin box and slider proxies
     */
    void setRange(double min, double max);
    double value() const;
    double minimum() const;
    void setMinimum(double min);
    double maximum() const;
    void setMaximum(double max);
    double singleStep() const;
    void setSingleStep(double singleStep);
    QString suffix() const;
    QString prefix() const;
    QString specialValueText() const;
    void setSpecialValueText(const QString &text);
    Qt::Alignment alignment() const;
    void setAlignment(const Qt::Alignment alignment);

    /**
     * @return number of decimals.
     * @see setDecimals()
     */
    int decimals() const;
    /**
     * Specifies the number of digits to use.
     */
    void setDecimals(int decimals);

    /**
     * @return if slider is enabled.
     * @default disabled
     * @see setSliderEnabled()
     */
    bool sliderEnabled() const;

    /**
     * @param enabled Show the slider
     */
    void setSliderEnabled(bool enabled);

    /**
     * Sets the spacing of tickmarks for the slider.
     *
     * @param single The single slider step.
     * @param page The page slider step.
     */
    void setSteps(int single, int page);

public Q_SLOTS:
    /**
     * Spin box and slider proxies
     */
    void setValue(double value);
    void setSuffix(const KLocalizedString &suffix);
    void setSuffix(const QString &suffix);
    void setPrefix(const QString &prefix);

Q_SIGNALS:
    void valueChanged(double);
    void editingFinished();

protected:
    // QWidget reimplementation
    virtual void changeEvent(QEvent *event);

private:
    friend KDoubleNumInputPrivate;
    KDoubleNumInputPrivate* const d;

    Q_PRIVATE_SLOT(d, void _k_valueChanged(double value));
    Q_PRIVATE_SLOT(d, void _k_sliderMoved(int value));

    Q_DISABLE_COPY(KDoubleNumInput)
};

#endif // K_NUMINPUT_H
