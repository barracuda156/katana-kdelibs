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

#ifndef PLASMA_RADIOBUTTON_H
#define PLASMA_RADIOBUTTON_H

#include <QtGui/QGraphicsProxyWidget>

#include <QRadioButton>

#include <plasma/plasma_export.h>

namespace Plasma
{

class RadioButtonPrivate;

/**
 * @class RadioButton plasma/widgets/radiobutton.h <Plasma/Widgets/RadioButton>
 *
 * @short Provides a plasma-themed QRadioButton.
 */
class PLASMA_EXPORT RadioButton : public QGraphicsProxyWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)

public:
    explicit RadioButton(QGraphicsWidget *parent = nullptr);
    ~RadioButton();

    /**
     * Sets the display text for this RadioButton
     *
     * @param text the text to display; should be translated.
     */
    void setText(const QString &text);

    /**
     * @return the display text
     */
    QString text() const;

    /**
     * @return the native widget wrapped by this RadioButton
     */
    QRadioButton *nativeWidget() const;

    /**
     * Sets the checked state.
     *
     * @param checked true if checked, false if not
     */
    void setChecked(bool checked);

    /**
     * @return the checked state
     */
    bool isChecked() const;

Q_SIGNALS:
    void toggled(bool);

protected:
    void changeEvent(QEvent *event);

private:
    RadioButtonPrivate * const d;
    Q_PRIVATE_SLOT(d, void setPalette())
};

} // namespace Plasma

#endif // multiple inclusion guard
