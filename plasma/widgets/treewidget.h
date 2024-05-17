/*
 *   Copyright 2008 Marco Martin <notmart@gmail.com>
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

#ifndef PLASMA_TREEWIDGET_H
#define PLASMA_TREEWIDGET_H

#include <QtGui/QGraphicsProxyWidget>

#include <plasma/plasma_export.h>

#include <QTreeWidget>
#include <QAbstractItemModel>

namespace Plasma
{

class TreeWidgetPrivate;

/**
 * @class TreeWidget plasma/widgets/treewidget.h <Plasma/Widgets/TreeWidget>
 *
 * @short Provides a plasma-themed QTreeWidget.
 */
class PLASMA_EXPORT TreeWidget : public QGraphicsProxyWidget
{
    Q_OBJECT

public:
    explicit TreeWidget(QGraphicsWidget *parent = 0);
    ~TreeWidget();

    /**
     * @return the native widget wrapped by this TreeWidget
     */
    QTreeWidget *nativeWidget() const;

private:
    TreeWidgetPrivate *const d;
};

}
#endif // multiple inclusion guard
