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

#ifndef KMESSAGEWIDGET_H
#define KMESSAGEWIDGET_H

#include <kdeui_export.h>

#include <QEvent>
#include <QWidget>

class KMessageWidgetPrivate;

/**
 * @short A widget to provide feedback or propose opportunistic interactions.
 *
 * KMessageWidget can be used to provide inline feedback or to implement opportunistic
 * interactions.
 *
 * As a feedback widget, KMessageWidget provides a less intrusive alternative to "OK Only" message
 * boxes. If you do not need the modalness of KMessageBox, consider using KMessageWidget instead.
 *
 * @author Ivailo Monev <xakepa10@gmail.com>
 * @since 4.7
 */
class KDEUI_EXPORT KMessageWidget : public QWidget
{
    Q_OBJECT
    Q_ENUMS(MessageType)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool closeButtonVisible READ isCloseButtonVisible WRITE setCloseButtonVisible)
    Q_PROPERTY(MessageType messageType READ messageType WRITE setMessageType)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
public:
    enum MessageType {
        Information,
        Warning,
        Error
    };

    /**
     * Constructs a KMessageWidget with the specified parent.
     */
    explicit KMessageWidget(QWidget *parent = nullptr);
    ~KMessageWidget();

    QString text() const;
    bool wordWrap() const;
    bool isCloseButtonVisible() const;
    MessageType messageType() const;

    /**
     * The icon shown on the left of the text. By default, no icon is shown.
     * @since 4.11
     */
    QIcon icon() const;

public Q_SLOTS:
    void setText(const QString &text);
    void setWordWrap(bool wordWrap);
    void setCloseButtonVisible(bool visible);
    void setMessageType(KMessageWidget::MessageType type);

    /**
     * Show the widget using an animation, unless KGlobalSettings::graphicsEffectLevel() does not
     * allow simple effects.
     */
    void animatedShow();

    /**
     * Hide the widget using an animation, unless KGlobalSettings::graphicsEffectLevel() does not
     * allow simple effects.
     */
    void animatedHide();

    /**
     * Define an icon to be shown on the left of the text
     * @since 4.11
     */
    void setIcon(const QIcon &icon);

Q_SIGNALS:
    /**
     * This signal is emitted when the user clicks a link in the text label. The URL referred to by
     * the href anchor is passed in contents.
     *
     * @param contents text of the href anchor
     * @see QLabel::linkActivated()
     * @since 4.10
     */
    void linkActivated(const QString &contents);

    /**
     * This signal is emitted when the user hovers over a link in the text label. The URL referred
     * to by the href anchor is passed in contents.
     *
     * @param contents text of the href anchor
     * @see QLabel::linkHovered()
     * @since 4.11
     */
    void linkHovered(const QString &contents);

protected:
    bool event(QEvent *event);

private:
    friend KMessageWidgetPrivate;
    KMessageWidgetPrivate *const d;
};

#endif // KMESSAGEWIDGET_H
