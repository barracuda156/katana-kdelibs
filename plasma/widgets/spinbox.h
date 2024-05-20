/*
 *   Copyright 2008 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2009 Davide Bettio <davide.bettio@kdemail.net>
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

#ifndef PLASMA_SPINBOX_H
#define PLASMA_SPINBOX_H

#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>

#include <plasma/plasma_export.h>

namespace Plasma
{

class SpinBoxPrivate;

/**
 * @class SpinBox plasma/widgets/slider.h <Plasma/Widgets/SpinBox>
 *
 * @short Provides a plasma-themed KDoubleNumInput.
 */
class PLASMA_EXPORT SpinBox : public QGraphicsProxyWidget
{
    Q_OBJECT
    Q_PROPERTY(double maximum READ maximum WRITE setMinimum)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)

public:
    explicit SpinBox(QGraphicsWidget *parent = 0);
    ~SpinBox();

    /**
     * @return the maximum value
     */
    double maximum() const;

    /**
     * @return the minimum value
     */
    double minimum() const;

    /**
     * @return the current value
     */
    double value() const;

    /**
     * @return the native widget wrapped by this SpinBox
     */
    QDoubleSpinBox *nativeWidget() const;

protected:
    void changeEvent(QEvent *event);
    void focusOutEvent(QFocusEvent *event);

public Q_SLOTS:
    /**
     * Sets the maximum value the slider can take.
     */
    void setMaximum(double maximum);

    /**
     * Sets the minimum value the slider can take.
     */
    void setMinimum(double minimum);

    /**
     * Sets the minimum and maximum values the slider can take.
     */
    void setRange(double minimum, double maximum);

    /**
     * Sets the value of the slider.
     *
     * If it is outside the range specified by minimum() and maximum(),
     * it will be adjusted to fit.
     */
    void setValue(double value);

Q_SIGNALS:
    /**
     * This signal is emitted when the user drags the slider.
     *
     * In fact, it is emitted whenever the sliderMoved(double) signal
     * of KIntSpinBox would be emitted.  See the Qt documentation for
     * more information.
     */
    void sliderMoved(double value);

    /**
     * This signal is emitted when the slider value has changed,
     * with the new slider value as argument.
     */
    void valueChanged(double value);

    /**
     * This signal is emitted when editing is finished. 
     * This happens when the spinbox loses focus and when enter is pressed.
     */
    void editingFinished();

private:
    Q_PRIVATE_SLOT(d, void setPalette())

    SpinBoxPrivate * const d;
};

} // namespace Plasma

#endif // multiple inclusion guard
