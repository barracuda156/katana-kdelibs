/*
 *   Copyright 2009 Marco Martin <notmart@gmail.com>
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

#include "scrollwidget.h"

#include <cmath>

//Qt
#include <QGraphicsSceneEvent>
#include <QGraphicsGridLayout>
#include <QGraphicsScene>
#include <QApplication>
#include <QEvent>
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTextBrowser>
#include <QLabel>

//KDE
#include <kmimetype.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <ktextedit.h>

//Plasma
#include <plasma/widgets/scrollbar.h>
#include <plasma/widgets/svgwidget.h>
#include <plasma/widgets/label.h>
#include <plasma/widgets/textedit.h>
#include <plasma/widgets/textbrowser.h>
#include <plasma/animator.h>
#include <plasma/svg.h>

static const qreal MaxVelocity = 2000;

// time it takes the widget to flick back to its bounds when overshot
static const qreal FixupDuration = 600;

namespace Plasma
{

class ScrollWidgetPrivate
{
public:
    ScrollWidgetPrivate(ScrollWidget *parent)
        : q(parent),
        scrollingWidget(nullptr),
        borderSvg(nullptr),
        topBorder(nullptr),
        bottomBorder(nullptr),
        leftBorder(nullptr),
        rightBorder(nullptr),
        layout(nullptr),
        verticalScrollBar(nullptr),
        verticalScrollBarPolicy(Qt::ScrollBarAsNeeded),
        horizontalScrollBar(nullptr),
        horizontalScrollBarPolicy(Qt::ScrollBarAsNeeded),
        wheelTimer(nullptr),
        flickAnimationX(nullptr),
        flickAnimationY(nullptr),
        directMoveAnimation(nullptr),
        hasOvershoot(true),
        overflowBordersVisible(true),
        alignment(Qt::AlignLeft | Qt::AlignTop)
    {
        fixupAnimation.groupX = nullptr;
        fixupAnimation.startX = nullptr;
        fixupAnimation.endX = nullptr;
        fixupAnimation.groupY = nullptr;
        fixupAnimation.startY = nullptr;
        fixupAnimation.endY = nullptr;
        fixupAnimation.snapX = nullptr;
        fixupAnimation.snapY = nullptr;
    }

    void commonConstructor()
    {
        q->setFocusPolicy(Qt::StrongFocus);
        layout = new QGraphicsGridLayout(q);
        q->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->setContentsMargins(0, 0, 0, 0);
        scrollingWidget = new QGraphicsWidget(q);
        scrollingWidget->setFlag(QGraphicsItem::ItemHasNoContents);
        scrollingWidget->installEventFilter(q);
        layout->addItem(scrollingWidget, 0, 0);
        borderSvg = new Plasma::Svg(q);
        borderSvg->setImagePath("widgets/scrollwidget");

        wheelTimer = new QTimer(q);
        wheelTimer->setSingleShot(true);

        verticalScrollBar = new Plasma::ScrollBar(q);
        verticalScrollBar->setFocusPolicy(Qt::NoFocus);
        layout->addItem(verticalScrollBar, 0, 1);
        verticalScrollBar->nativeWidget()->setMinimum(0);
        verticalScrollBar->nativeWidget()->setMaximum(100);
        QObject::connect(verticalScrollBar, SIGNAL(valueChanged(int)), q, SLOT(verticalScroll(int)));

        horizontalScrollBar = new Plasma::ScrollBar(q);
        verticalScrollBar->setFocusPolicy(Qt::NoFocus);
        horizontalScrollBar->setOrientation(Qt::Horizontal);
        layout->addItem(horizontalScrollBar, 1, 0);
        horizontalScrollBar->nativeWidget()->setMinimum(0);
        horizontalScrollBar->nativeWidget()->setMaximum(100);
        QObject::connect(horizontalScrollBar, SIGNAL(valueChanged(int)), q, SLOT(horizontalScroll(int)));

        layout->setColumnSpacing(0, 0);
        layout->setColumnSpacing(1, 0);
        layout->setRowSpacing(0, 0);
        layout->setRowSpacing(1, 0);
    }

    void adjustScrollbars()
    {
        if (!widget) {
            return;
        }

        const bool verticalVisible = widget.data()->size().height() > q->size().height();
        const bool horizontalVisible = widget.data()->size().width() > q->size().width();

        verticalScrollBar->nativeWidget()->setMaximum(qMax(0, int((widget.data()->size().height() - scrollingWidget->size().height())/10)));
        verticalScrollBar->nativeWidget()->setPageStep(int(scrollingWidget->size().height())/10);

        if (verticalScrollBarPolicy == Qt::ScrollBarAlwaysOff ||
            !verticalVisible) {
            if (layout->count() > 2 && layout->itemAt(2) == verticalScrollBar) {
                layout->removeAt(2);
            } else if (layout->count() > 1 && layout->itemAt(1) == verticalScrollBar) {
                layout->removeAt(1);
            }
            verticalScrollBar->hide();
        } else if (!verticalScrollBar->isVisible()) {
            layout->addItem(verticalScrollBar, 0, 1);
            verticalScrollBar->show();
        }

        horizontalScrollBar->nativeWidget()->setMaximum(qMax(0, int((widget.data()->size().width() - scrollingWidget->size().width())/10)));
        horizontalScrollBar->nativeWidget()->setPageStep(int(scrollingWidget->size().width())/10);

        if (horizontalScrollBarPolicy == Qt::ScrollBarAlwaysOff ||
            !horizontalVisible) {
            if (layout->count() > 2 && layout->itemAt(2) == horizontalScrollBar) {
                layout->removeAt(2);
            } else if (layout->count() > 1 && layout->itemAt(1) == horizontalScrollBar) {
                layout->removeAt(1);
            }
            horizontalScrollBar->hide();
        } else if (!horizontalScrollBar->isVisible()) {
            layout->addItem(horizontalScrollBar, 1, 0);
            horizontalScrollBar->show();
        }

         if (widget && !topBorder && verticalVisible) {
            topBorder = new Plasma::SvgWidget(q);
            topBorder->setSvg(borderSvg);
            topBorder->setElementID("border-top");
            topBorder->setZValue(900);
            topBorder->resize(topBorder->effectiveSizeHint(Qt::PreferredSize));
            topBorder->setVisible(overflowBordersVisible);

            bottomBorder = new Plasma::SvgWidget(q);
            bottomBorder->setSvg(borderSvg);
            bottomBorder->setElementID("border-bottom");
            bottomBorder->setZValue(900);
            bottomBorder->resize(bottomBorder->effectiveSizeHint(Qt::PreferredSize));
            bottomBorder->setVisible(overflowBordersVisible);
        } else if (topBorder && widget && !verticalVisible) {
            //FIXME: in some cases topBorder->deleteLater() is deleteNever(), why?
            topBorder->hide();
            bottomBorder->hide();
            topBorder->deleteLater();
            bottomBorder->deleteLater();
            topBorder = 0;
            bottomBorder = 0;
        }


        if (widget && !leftBorder && horizontalVisible) {
            leftBorder = new Plasma::SvgWidget(q);
            leftBorder->setSvg(borderSvg);
            leftBorder->setElementID("border-left");
            leftBorder->setZValue(900);
            leftBorder->resize(leftBorder->effectiveSizeHint(Qt::PreferredSize));
            leftBorder->setVisible(overflowBordersVisible);

            rightBorder = new Plasma::SvgWidget(q);
            rightBorder->setSvg(borderSvg);
            rightBorder->setElementID("border-right");
            rightBorder->setZValue(900);
            rightBorder->resize(rightBorder->effectiveSizeHint(Qt::PreferredSize));
            rightBorder->setVisible(overflowBordersVisible);
        } else if (leftBorder && widget && !horizontalVisible) {
            leftBorder->hide();
            rightBorder->hide();
            leftBorder->deleteLater();
            rightBorder->deleteLater();
            leftBorder = 0;
            rightBorder = 0;
        }

        layout->activate();

        if (topBorder) {
            topBorder->resize(q->size().width(), topBorder->size().height());
            bottomBorder->resize(q->size().width(), bottomBorder->size().height());
            bottomBorder->setPos(0, q->size().height() - topBorder->size().height());
        }
        if (leftBorder) {
            leftBorder->resize(leftBorder->size().width(), q->size().height());
            rightBorder->resize(rightBorder->size().width(), q->size().height());
            rightBorder->setPos(q->size().width() - rightBorder->size().width(), 0);
        }

        QSizeF widgetSize = widget.data()->size();
        if (widget.data()->sizePolicy().expandingDirections() & Qt::Horizontal) {
            //keep a 1 pixel border
            widgetSize.setWidth(scrollingWidget->size().width());
        }
        if (widget.data()->sizePolicy().expandingDirections() & Qt::Vertical) {
            widgetSize.setHeight(scrollingWidget->size().height());
        }
        widget.data()->resize(widgetSize);

        adjustClipping();
    }

    void verticalScroll(int value)
    {
        if (!widget) {
            return;
        }

        widget.data()->setPos(QPoint(widget.data()->pos().x(), -value*10));
    }

    void horizontalScroll(int value)
    {
        if (!widget) {
            return;
        }

        widget.data()->setPos(QPoint(-value*10, widget.data()->pos().y()));
    }

    void adjustClipping()
    {
        if (!widget) {
            return;
        }

        const bool clip = widget.data()->size().width() > scrollingWidget->size().width() || widget.data()->size().height() > scrollingWidget->size().height();

        scrollingWidget->setFlag(QGraphicsItem::ItemClipsChildrenToShape, clip);
    }

    qreal overShootDistance(qreal velocity, qreal size) const
    {
        if (MaxVelocity <= 0)
            return 0.0;

        velocity = qAbs(velocity);
        if (velocity > MaxVelocity)
            velocity = MaxVelocity;
        qreal dist = size / 4 * velocity / MaxVelocity;
        return dist;
    }

    void animateMoveTo(const QPointF &pos)
    {
        qreal duration = 800;
        QPointF start = q->scrollPosition();
        QSizeF threshold = q->viewportGeometry().size();
        QPointF diff = pos - start;

        // reduce if it's within the viewport
        if (qAbs(diff.x()) < threshold.width() ||
            qAbs(diff.y()) < threshold.height())
            duration /= 2;

        fixupAnimation.groupX->stop();
        fixupAnimation.groupY->stop();
        fixupAnimation.snapX->stop();
        fixupAnimation.snapY->stop();

        directMoveAnimation->setStartValue(start);
        directMoveAnimation->setEndValue(pos);
        directMoveAnimation->setDuration(duration);
        directMoveAnimation->start();
    }

    void flick(QPropertyAnimation *anim,
               qreal velocity,
               qreal val,
               qreal minExtent,
               qreal maxExtent,
               qreal size)
    {
        qreal deceleration = 500;
        qreal maxDistance = -1;
        qreal target = 0;
        // -ve velocity means list is moving up
        if (velocity > 0) {
            if (val < minExtent)
                maxDistance = qAbs(minExtent - val + (hasOvershoot ? overShootDistance(velocity,size) : 0));
            target = minExtent;
            deceleration = -deceleration;
        } else {
            if (val > maxExtent)
                maxDistance = qAbs(maxExtent - val) + (hasOvershoot ? overShootDistance(velocity,size) : 0);
            target = maxExtent;
        }
        if (maxDistance > 0) {
            qreal v = velocity;
            if (MaxVelocity != -1 && MaxVelocity < qAbs(v)) {
                if (v < 0)
                    v = -MaxVelocity;
                else
                    v = MaxVelocity;
            }
            qreal duration = qAbs(v / deceleration);
            qreal diffY = v * duration + (0.5  * deceleration * duration * duration);
            qreal startY = val;

            qreal endY = startY + diffY;

            if (velocity > 0) {
                if (endY > target)
                    endY = startY + maxDistance;
            } else {
                if (endY < target)
                    endY = startY - maxDistance;
            }
            duration = qAbs((endY-startY)/ (-v/2));

#ifndef NDEBUG
            qDebug() <<"XXX velocity = "<<v <<", target = "<< target
                     <<", maxDist = "<<maxDistance;
            qDebug() <<"duration = "<<duration<<" secs, ("
                     << (duration * 1000) <<" msecs)";
            qDebug() <<"startY = "<<startY;
            qDebug() <<"endY = "<<endY;
            qDebug() <<"overshoot = " << overShootDistance(v, size);
            qDebug() <<"avg velocity = " << ((endY-startY) / duration);
#endif

            anim->setStartValue(startY);
            anim->setEndValue(endY);
            anim->setDuration(duration * 1000);
            anim->start();
        } else {
            if (anim == flickAnimationX)
                fixupX();
            else
                fixupY();
        }
    }

    void flickX(qreal velocity)
    {
        flick(flickAnimationX, velocity, widget.data()->x(), minXExtent(), maxXExtent(),
              q->viewportGeometry().width());
    }

    void flickY(qreal velocity)
    {
        flick(flickAnimationY, velocity, widget.data()->y(),minYExtent(), maxYExtent(),
              q->viewportGeometry().height());
    }

    void fixup(QAnimationGroup *group,
               QPropertyAnimation *start, QPropertyAnimation *end,
               qreal val, qreal minExtent, qreal maxExtent)
    {
        if (val > minExtent || maxExtent > minExtent) {
            if (!qFuzzyCompare(val, minExtent)) {
                if (FixupDuration) {
                    qreal dist = minExtent - val;
                    start->setStartValue(val);
                    start->setEndValue(minExtent - dist/2);
                    end->setStartValue(minExtent - dist/2);
                    end->setEndValue(minExtent);
                    start->setDuration(FixupDuration/4);
                    end->setDuration(3*FixupDuration/4);
                    group->start();
                } else {
                    QObject *obj = start->targetObject();
                    obj->setProperty(start->propertyName(), minExtent);
                }
            }
        } else if (val < maxExtent) {
            if (FixupDuration) {
                qreal dist = maxExtent - val;
                start->setStartValue(val);
                start->setEndValue(maxExtent - dist/2);
                end->setStartValue(maxExtent - dist/2);
                end->setEndValue(maxExtent);
                start->setDuration(FixupDuration/4);
                end->setDuration(3*FixupDuration/4);
                group->start();
            } else {
                QObject *obj = start->targetObject();
                obj->setProperty(start->propertyName(), maxExtent);
            }
        } else if (end == fixupAnimation.endX && snapSize.width() > 1 &&
                   q->contentsSize().width() > q->viewportGeometry().width()) {
            int target = snapSize.width() * round(val/snapSize.width());
            fixupAnimation.snapX->setStartValue(val);
            fixupAnimation.snapX->setEndValue(target);
            fixupAnimation.snapX->setDuration(FixupDuration);
            fixupAnimation.snapX->start();
        } else if (end == fixupAnimation.endY && snapSize.height() > 1 &&
                   q->contentsSize().height() > q->viewportGeometry().height()) {
            int target = snapSize.height() * round(val/snapSize.height());
            fixupAnimation.snapY->setStartValue(val);
            fixupAnimation.snapY->setEndValue(target);
            fixupAnimation.snapY->setDuration(FixupDuration);
            fixupAnimation.snapY->start();
        }
    }
    void fixupX()
    {
        fixup(
            fixupAnimation.groupX, fixupAnimation.startX, fixupAnimation.endX,
            widget.data()->x(), minXExtent(), maxXExtent()
        );
    }

    void fixupY()
    {
        fixup(
            fixupAnimation.groupY, fixupAnimation.startY, fixupAnimation.endY,
            widget.data()->y(), minYExtent(), maxYExtent()
        );
    }

    void makeRectVisible(const QRectF &rectToBeVisible)
    {
        if (!widget) {
            return;
        }

        QRectF viewRect = scrollingWidget->boundingRect();
        //ensure the rect is not outside the widget bounding rect
        QRectF mappedRect = QRectF(
            QPointF(
                qBound((qreal)0.0, rectToBeVisible.x(), widget.data()->size().width() - rectToBeVisible.width()),
                qBound((qreal)0.0, rectToBeVisible.y(), widget.data()->size().height() - rectToBeVisible.height())
            ),
            rectToBeVisible.size()
        );
        mappedRect = widget.data()->mapToItem(scrollingWidget, mappedRect).boundingRect();

        if (viewRect.contains(mappedRect)) {
            return;
        }

        QPointF delta(0, 0);

        if (mappedRect.top() < 0) {
            delta.setY(-mappedRect.top());
        } else if  (mappedRect.bottom() > viewRect.bottom()) {
            delta.setY(viewRect.bottom() - mappedRect.bottom());
        }

        if (mappedRect.left() < 0) {
            delta.setX(-mappedRect.left());
        } else if  (mappedRect.right() > viewRect.right()) {
            delta.setX(viewRect.right() - mappedRect.right());
        }

        animateMoveTo(q->scrollPosition() - delta);
    }

    void makeItemVisible(QGraphicsItem *itemToBeVisible)
    {
        if (!widget) {
            return;
        }

        QRectF rect(widget.data()->mapFromScene(itemToBeVisible->scenePos()), itemToBeVisible->boundingRect().size());
        makeRectVisible(rect);
    }

    void makeItemVisible()
    {
        if (widgetToBeVisible) {
            makeItemVisible(widgetToBeVisible.data());
        }
    }

    void stopAnimations()
    {
        flickAnimationX->stop();
        flickAnimationY->stop();
        fixupAnimation.groupX->stop();
        fixupAnimation.groupY->stop();
    }

    void handleKeyPressEvent(QKeyEvent *event)
    {
        if (!widget.data()) {
            event->ignore();
            return;
        }

        QPointF start = q->scrollPosition();
        QPointF end = start;

        qreal step = 100;

        switch (event->key()) {
            case Qt::Key_Left: {
                if (canXFlick()) {
                    end += QPointF(-step, 0);
                }
                break;
            }
            case Qt::Key_Right: {
                if (canXFlick()) {
                    end += QPointF(step, 0);
                }
                break;
            }
            case Qt::Key_Up: {
                if (canYFlick()) {
                    end += QPointF(0, -step);
                }
                break;
            }
            case Qt::Key_Down: {
                if (canYFlick()) {
                    end += QPointF(0, step);
                }
                break;
            }
            default: {
                event->ignore();
                return;
            }
        }

        fixupAnimation.groupX->stop();
        fixupAnimation.groupY->stop();
        fixupAnimation.snapX->stop();
        fixupAnimation.snapY->stop();
        directMoveAnimation->setStartValue(start);
        directMoveAnimation->setEndValue(end);
        directMoveAnimation->setDuration(200);
        directMoveAnimation->start();
    }

    void handleWheelEvent(QGraphicsSceneWheelEvent *event)
    {
        // only scroll when the animation is done, this avoids to receive too many events and
        // getting mad when they arrive from a touchpad
        if (!widget.data() || wheelTimer->isActive()) {
            return;
        }

        QPointF start = q->scrollPosition();
        QPointF end = start;

        //At some point we should switch to
        // step = QApplication::wheelScrollLines() *
        //      (event->delta()/120) *
        //      scrollBar->singleStep();
        // which gives us exactly the number of lines to scroll but the issue
        // is that at this point we don't have any clue what a "line" is and if
        // we make it a pixel then scrolling by 3 (default) pixels will be
        // very painful
        qreal step = -event->delta()/3;

        // if the widget can scroll in a single axis and the wheel is the other one, scroll the other one
        if (event->orientation() == Qt::Vertical) {
            if (!canYFlick() && canXFlick()) {
                end += QPointF(step, 0);
            } else if (canYFlick()) {
                end += QPointF(0, step);
            } else {
                return;
            }
        } else {
            if (canYFlick() && !canXFlick()) {
                end += QPointF(0, step);
            } else if (canXFlick()) {
                end += QPointF(step, 0);
            } else {
                return;
            }
        }

        fixupAnimation.groupX->stop();
        fixupAnimation.groupY->stop();
        fixupAnimation.snapX->stop();
        fixupAnimation.snapY->stop();
        directMoveAnimation->setStartValue(start);
        directMoveAnimation->setEndValue(end);
        directMoveAnimation->setDuration(200);
        directMoveAnimation->start();
        wheelTimer->start(50);
    }

    qreal minXExtent() const
    {
        if (alignment & Qt::AlignLeft) {
            return 0;
        }
        qreal vWidth = q->viewportGeometry().width();
        qreal cWidth = q->contentsSize().width();
        if (cWidth < vWidth) {
            if (alignment & Qt::AlignRight) {
                return  vWidth - cWidth;
            } else if (alignment & Qt::AlignHCenter) {
                return vWidth / 2 - cWidth / 2;
            }
        }
        return 0;
    }

    qreal maxXExtent() const
    {
        return q->viewportGeometry().width() - q->contentsSize().width();
    }

    qreal minYExtent() const
    {
        if (alignment & Qt::AlignTop) {
            return 0;
        }
        qreal vHeight = q->viewportGeometry().height();
        qreal cHeight = q->contentsSize().height();
        if (cHeight < vHeight) {
            if (alignment & Qt::AlignBottom) {
                return  vHeight - cHeight;
            } else if (alignment & Qt::AlignVCenter) {
                return vHeight / 2 - cHeight / 2;
            }
        }
        return 0;
    }

    qreal maxYExtent() const
    {
        return q->viewportGeometry().height() - q->contentsSize().height();
    }

    bool canXFlick() const
    {
        // make the thing feel quite "fixed" don't permit to flick when the contents size is less than the viewport
        return q->contentsSize().width() > q->viewportGeometry().width();
    }

    bool canYFlick() const
    {
        return q->contentsSize().height() > q->viewportGeometry().height();
    }

    void createFlickAnimations()
    {
        if (widget.data()) {
            const QByteArray xProp("x");
            const QByteArray yProp("y");

            flickAnimationX = new QPropertyAnimation(widget.data(), xProp, widget.data());
            flickAnimationY = new QPropertyAnimation(widget.data(), yProp, widget.data());
            QObject::connect(flickAnimationX, SIGNAL(finished()), q, SLOT(fixupX()));
            QObject::connect(flickAnimationY, SIGNAL(finished()), q, SLOT(fixupY()));

            QObject::connect(
                flickAnimationX, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
                q, SIGNAL(scrollStateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            );
            QObject::connect(
                flickAnimationY, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
                q, SIGNAL(scrollStateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            );

            flickAnimationX->setEasingCurve(QEasingCurve::OutCirc);
            flickAnimationY->setEasingCurve(QEasingCurve::OutCirc);

            fixupAnimation.groupX = new QSequentialAnimationGroup(widget.data());
            fixupAnimation.groupY = new QSequentialAnimationGroup(widget.data());
            fixupAnimation.startX = new QPropertyAnimation(widget.data(), xProp, widget.data());
            fixupAnimation.startY = new QPropertyAnimation(widget.data(), yProp, widget.data());
            fixupAnimation.endX = new QPropertyAnimation(widget.data(), xProp, widget.data());
            fixupAnimation.endY = new QPropertyAnimation(widget.data(), yProp, widget.data());
            fixupAnimation.groupX->addAnimation(fixupAnimation.startX);
            fixupAnimation.groupY->addAnimation(fixupAnimation.startY);
            fixupAnimation.groupX->addAnimation(fixupAnimation.endX);
            fixupAnimation.groupY->addAnimation(fixupAnimation.endY);

            fixupAnimation.startX->setEasingCurve(QEasingCurve::InQuad);
            fixupAnimation.endX->setEasingCurve(QEasingCurve::OutQuint);
            fixupAnimation.startY->setEasingCurve(QEasingCurve::InQuad);
            fixupAnimation.endY->setEasingCurve(QEasingCurve::OutQuint);

            fixupAnimation.snapX = new QPropertyAnimation(widget.data(), xProp, widget.data());
            fixupAnimation.snapY = new QPropertyAnimation(widget.data(), yProp, widget.data());
            fixupAnimation.snapX->setEasingCurve(QEasingCurve::InOutQuad);
            fixupAnimation.snapY->setEasingCurve(QEasingCurve::InOutQuad);

            QObject::connect(
                fixupAnimation.groupX, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
                q, SIGNAL(scrollStateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            );
            QObject::connect(
                fixupAnimation.groupY, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
                q, SIGNAL(scrollStateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            );

            directMoveAnimation = new QPropertyAnimation(q, "scrollPosition", q);
            QObject::connect(directMoveAnimation, SIGNAL(finished()), q, SLOT(fixupX()));
            QObject::connect(directMoveAnimation, SIGNAL(finished()), q, SLOT(fixupY()));
            QObject::connect(
                directMoveAnimation, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)),
                q, SIGNAL(scrollStateChanged(QAbstractAnimation::State, QAbstractAnimation::State))
            );
            directMoveAnimation->setEasingCurve(QEasingCurve::OutCirc);
        }
    }

    void deleteFlickAnimations()
    {
        if (flickAnimationX) {
            flickAnimationX->stop();
        }
        if (flickAnimationY) {
            flickAnimationY->stop();
        }
        delete flickAnimationX;
        flickAnimationX = nullptr;
        delete flickAnimationY;
        flickAnimationY = nullptr;
        delete fixupAnimation.groupX;
        fixupAnimation.groupX = nullptr;
        delete fixupAnimation.groupY;
        fixupAnimation.groupY = nullptr;
        delete directMoveAnimation;
        directMoveAnimation = nullptr;
        delete fixupAnimation.snapX;
        fixupAnimation.snapX = nullptr;
        delete fixupAnimation.snapY;
        fixupAnimation.snapY = nullptr;
    }

    void setScrollX()
    {
        if (horizontalScrollBarPolicy != Qt::ScrollBarAlwaysOff) {
            horizontalScrollBar->blockSignals(true);
            horizontalScrollBar->setValue(-widget.data()->pos().x() / 10.0);
            horizontalScrollBar->blockSignals(false);
        }
    }

    void setScrollY()
    {
        if (verticalScrollBarPolicy != Qt::ScrollBarAlwaysOff) {
            verticalScrollBar->blockSignals(true);
            verticalScrollBar->setValue(-widget.data()->pos().y() / 10.0);
            verticalScrollBar->blockSignals(false);
        }
    }

    ScrollWidget *q;
    QGraphicsWidget *scrollingWidget;
    QWeakPointer<QGraphicsWidget> widget;
    Plasma::Svg *borderSvg;
    Plasma::SvgWidget *topBorder;
    Plasma::SvgWidget *bottomBorder;
    Plasma::SvgWidget *leftBorder;
    Plasma::SvgWidget *rightBorder;
    QGraphicsGridLayout *layout;
    ScrollBar *verticalScrollBar;
    Qt::ScrollBarPolicy verticalScrollBarPolicy;
    ScrollBar *horizontalScrollBar;
    Qt::ScrollBarPolicy horizontalScrollBarPolicy;
    QWeakPointer<QGraphicsWidget> widgetToBeVisible;
    QTimer *wheelTimer;

    QPropertyAnimation *flickAnimationX;
    QPropertyAnimation *flickAnimationY;
    struct {
        QAnimationGroup *groupX;
        QPropertyAnimation *startX;
        QPropertyAnimation *endX;

        QAnimationGroup *groupY;
        QPropertyAnimation *startY;
        QPropertyAnimation *endY;

        QPropertyAnimation *snapX;
        QPropertyAnimation *snapY;
    } fixupAnimation;
    QPropertyAnimation *directMoveAnimation;
    QSizeF snapSize;
    bool hasOvershoot;
    bool overflowBordersVisible;

    Qt::Alignment alignment;
};


ScrollWidget::ScrollWidget(QGraphicsItem *parent)
    : QGraphicsWidget(parent),
    d(new ScrollWidgetPrivate(this))
{
    d->commonConstructor();
}

ScrollWidget::ScrollWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    d(new ScrollWidgetPrivate(this))
{
    d->commonConstructor();
}

ScrollWidget::~ScrollWidget()
{
    delete d;
}

void ScrollWidget::setWidget(QGraphicsWidget *widget)
{
    if (d->widget && d->widget.data() != widget) {
        d->deleteFlickAnimations();
        d->widget.data()->removeEventFilter(this);
    }

    d->widget = widget;
    // it's not good it's setting a size policy here, but it's done to be retrocompatible with older applications
    if (widget) {
        d->createFlickAnimations();

        connect(widget, SIGNAL(xChanged()), this, SLOT(setScrollX()));
        connect(widget, SIGNAL(yChanged()), this, SLOT(setScrollY()));
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        widget->setParentItem(d->scrollingWidget);
        widget->setPos(d->minXExtent(), d->minYExtent());
        widget->installEventFilter(this);
        d->adjustScrollbars();
    }
}

QGraphicsWidget *ScrollWidget::widget() const
{
    return d->widget.data();
}

void ScrollWidget::setHorizontalScrollBarPolicy(const Qt::ScrollBarPolicy policy)
{
    d->horizontalScrollBarPolicy = policy;
}

Qt::ScrollBarPolicy ScrollWidget::horizontalScrollBarPolicy() const
{
    return d->horizontalScrollBarPolicy;
}

void ScrollWidget::setVerticalScrollBarPolicy(const Qt::ScrollBarPolicy policy)
{
    d->verticalScrollBarPolicy = policy;
}

Qt::ScrollBarPolicy ScrollWidget::verticalScrollBarPolicy() const
{
    return d->verticalScrollBarPolicy;
}

bool ScrollWidget::overflowBordersVisible() const
{
    return d->overflowBordersVisible;
}

void ScrollWidget::setOverflowBordersVisible(const bool visible)
{
    if (d->overflowBordersVisible == visible) {
        return;
    }

    d->overflowBordersVisible = visible;
    d->adjustScrollbars();
}

void ScrollWidget::ensureRectVisible(const QRectF &rect)
{
    if (!d->widget) {
        return;
    }

    d->makeRectVisible(rect);
}

void ScrollWidget::ensureItemVisible(QGraphicsItem *item)
{
    if (!d->widget || !item) {
        return;
    }

    QGraphicsItem *parentOfItem = item->parentItem();
    while (parentOfItem != d->widget.data()) {
        if (!parentOfItem) {
            return;
        }

        parentOfItem = parentOfItem->parentItem();
    }

    // may or may not be valid, delay only if it's a qgraphicswidget
    QGraphicsWidget *widget = qgraphicsitem_cast<QGraphicsWidget *>(item);
    if (widget) {
        d->widgetToBeVisible = widget;

        // need to wait for the parent item to resize...
        QTimer::singleShot(0, this, SLOT(makeItemVisible()));
    } else {
        d->makeItemVisible(item);
    }
}

QRectF ScrollWidget::viewportGeometry() const
{
    if (d->widget) {
        return d->scrollingWidget->boundingRect();
    }
    return QRectF();
}

QSizeF ScrollWidget::contentsSize() const
{
    if (d->widget) {
        return d->widget.data()->size();
    }
    return QSizeF();
}

void ScrollWidget::setScrollPosition(const QPointF &position)
{
    if (d->widget) {
        d->widget.data()->setPos(-position.toPoint());
    }
}

QPointF ScrollWidget::scrollPosition() const
{
    if (d->widget) {
        return -d->widget.data()->pos();
    }
    return QPointF();
}

void ScrollWidget::setSnapSize(const QSizeF &size)
{
    d->snapSize = size;
}

QSizeF ScrollWidget::snapSize() const
{
    return d->snapSize;
}

void ScrollWidget::focusInEvent(QFocusEvent *event)
{
    if (d->widget) {
        d->widget.data()->setFocus(event->reason());
    }
}

void ScrollWidget::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    if (!d->widget) {
        QGraphicsWidget::resizeEvent(event);
        return;
    }

    d->adjustScrollbars();

    //if topBorder exists bottomBorder too
    if (d->topBorder) {
        d->topBorder->resize(event->newSize().width(), d->topBorder->size().height());
        d->bottomBorder->resize(event->newSize().width(), d->bottomBorder->size().height());
        d->bottomBorder->setPos(0, event->newSize().height() - d->bottomBorder->size().height());
    }
    if (d->leftBorder) {
        d->leftBorder->resize(d->leftBorder->size().width(), event->newSize().height());
        d->rightBorder->resize(d->rightBorder->size().width(), event->newSize().height());
        d->rightBorder->setPos(event->newSize().width() - d->rightBorder->size().width(), 0);
    }

    QGraphicsWidget::resizeEvent(event);
}

void ScrollWidget::keyPressEvent(QKeyEvent *event)
{
    d->handleKeyPressEvent(event);
}

void ScrollWidget::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (!d->widget) {
        return;
    } else if (!d->canYFlick() && !d->canXFlick()) {
        event->ignore();
        return;
    }
    d->handleWheelEvent(event);
    event->accept();
}

bool ScrollWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (!d->widget) {
        return false;
    }

    if (watched == d->scrollingWidget && (event->type() == QEvent::GraphicsSceneResize ||
         event->type() == QEvent::Move)) {
        emit viewportGeometryChanged(viewportGeometry());
    } else if (watched == d->widget.data() && event->type() == QEvent::GraphicsSceneResize) {
        d->stopAnimations();
        d->adjustScrollbars();
        updateGeometry();

        QPointF newPos = d->widget.data()->pos();
        if (d->widget.data()->size().width() <= viewportGeometry().width()) {
            newPos.setX(d->minXExtent());
        }
        if (d->widget.data()->size().height() <= viewportGeometry().height()) {
            newPos.setY(d->minYExtent());
        }
        //check if the content is visible
        if (d->widget.data()->geometry().right() < 0) {
            newPos.setX(-d->widget.data()->geometry().width()+viewportGeometry().width());
        }
        if (d->widget.data()->geometry().bottom() < 0) {
            newPos.setY(-d->widget.data()->geometry().height()+viewportGeometry().height());
        }
        d->widget.data()->setPos(newPos);

    } else if (watched == d->widget.data() && event->type() == QEvent::GraphicsSceneMove) {
        d->horizontalScrollBar->blockSignals(true);
        d->verticalScrollBar->blockSignals(true);
        d->horizontalScrollBar->setValue(-d->widget.data()->pos().x()/10);
        d->verticalScrollBar->setValue(-d->widget.data()->pos().y()/10);
        d->horizontalScrollBar->blockSignals(false);
        d->verticalScrollBar->blockSignals(false);
    }

    return false;
}

QSizeF ScrollWidget::sizeHint(Qt::SizeHint which, const QSizeF & constraint) const
{
    if (!d->widget || which == Qt::MaximumSize) {
        return QGraphicsWidget::sizeHint(which, constraint);
    //FIXME: it should ake the minimum hint of the contained widget, but the result is in a ridiculously big widget
    } else if (which == Qt::MinimumSize) {
        return QSizeF(KIconLoader::SizeEnormous, KIconLoader::SizeEnormous);
    }

    QSizeF hint = d->widget.data()->effectiveSizeHint(which, constraint);
    if (d->horizontalScrollBar && d->horizontalScrollBar->isVisible()) {
        hint += QSize(0, d->horizontalScrollBar->size().height());
    }
    if (d->verticalScrollBar && d->verticalScrollBar->isVisible()) {
        hint += QSize(d->verticalScrollBar->size().width(), 0);
    }

    return hint;
}

void Plasma::ScrollWidget::setAlignment(Qt::Alignment align)
{
    d->alignment = align;
    if (d->widget.data() && d->widget.data()->isVisible()) {
        d->widget.data()->setPos(d->minXExtent(), d->minYExtent());
    }
}

Qt::Alignment Plasma::ScrollWidget::alignment() const
{
    return d->alignment;
}

void ScrollWidget::setOverShoot(bool enable)
{
    d->hasOvershoot = enable;
}

bool ScrollWidget::hasOverShoot() const
{
    return d->hasOvershoot;
}

} // namespace Plasma


#include "moc_scrollwidget.cpp"

