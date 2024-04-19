/*
 *   Copyright 2023 Ivailo Monev <xakepa10@gmail.com>
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

#ifndef PLASMA_CALENDARWIDGET_H
#define PLASMA_CALENDARWIDGET_H

#include <QtGui/QGraphicsProxyWidget>

#include <plasma/plasma_export.h>

class KCalendarWidget;

namespace Plasma
{

class CalendarWidgetPrivate;

/**
 * @class CalendarWidget plasma/widgets/calendarwidget.h <Plasma/Widgets/CalendarWidget>
 *
 * @short Provides a plasma-themed KCalendarWidget.
 */
class PLASMA_EXPORT CalendarWidget : public QGraphicsProxyWidget
{
    Q_OBJECT
    Q_PROPERTY(QDate selectedDate READ selectedDate WRITE setSelectedDate)

public:
    explicit CalendarWidget(QGraphicsWidget *parent = 0);
    ~CalendarWidget();

    /**
     * @return the date selected
     */
    QDate selectedDate() const;

    /**
     * Sets the selected data
     *
     * @param date the date to display
     */
    void setSelectedDate(const QDate &date);

    /**
     * @return the native widget wrapped by this CalendarWidget
     */
    KCalendarWidget* nativeWidget() const;

Q_SIGNALS:
    void clicked(QDate);
    void activated(QDate);

protected:
    void focusInEvent(QFocusEvent *event);

private:
    Q_PRIVATE_SLOT(d, void setPalette())

    CalendarWidgetPrivate *const d;
};

}
#endif // multiple inclusion guard
