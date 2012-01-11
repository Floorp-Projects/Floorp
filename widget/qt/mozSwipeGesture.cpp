/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Miika Jarvinen <mjarvin@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "mozSwipeGesture.h"
#include <QTouchEvent>
#include <QGraphicsWidget>
#include <prtypes.h>
#include <nsIDOMSimpleGestureEvent.h>
#include <math.h>

// Percent of screen size
static const float TRIGGER_DISTANCE = 0.3;

// It would be nice to get platform defines for these values
// Maximum finger distance in pixels
const int MAX_FINGER_DISTANCE = 250;
// We could define it as 2*QT_GUI_DOUBLE_CLICK_RADIUS, but it's not available
// due to QTBUG-9630
const int FINGER_DISTANCE_MISTAKE = 50;

QGesture*
MozSwipeGestureRecognizer::create(QObject* target)
{
    return new MozSwipeGesture();
}

QGestureRecognizer::Result
MozSwipeGestureRecognizer::recognize(QGesture* aState,
                                     QObject* aWatched,
                                     QEvent* aEvent)
{
    const QTouchEvent* ev = static_cast<const QTouchEvent *>(aEvent);
    MozSwipeGesture* swipe = static_cast<MozSwipeGesture *>(aState);

    QGestureRecognizer::Result result = QGestureRecognizer::Ignore;

    QGraphicsWidget* widget = qobject_cast<QGraphicsWidget*>(aWatched);
    if (!widget) {
        return result;
    }

    switch (aEvent->type()) {
        case QEvent::TouchBegin:
            swipe->mSwipeState = MozSwipeGesture::NOT_STARTED;
            result = QGestureRecognizer::MayBeGesture;
            break;

        case QEvent::TouchEnd:
            if (swipe->state() != Qt::NoGesture &&
                swipe->mSwipeState == MozSwipeGesture::TRIGGERED) {
                result = QGestureRecognizer::FinishGesture;
            }
            else {
                result = QGestureRecognizer::CancelGesture;
            }
            break;

        case QEvent::TouchUpdate:
            // We have already handled this swipe
            if (swipe->mSwipeState > MozSwipeGesture::STARTED) {
                break;
            }

            if (ev->touchPoints().count() > 2) {
                swipe->mSwipeState = MozSwipeGesture::CANCELLED;
                result = QGestureRecognizer::CancelGesture;
                break;
            }


            if (ev->touchPoints().count() == 2) {
               swipe->mSwipeState = MozSwipeGesture::STARTED;
               QList <QTouchEvent::TouchPoint> touchPoints = ev->touchPoints();
               if (!swipe->Update(touchPoints[0], touchPoints[1], widget->size())) {
                   result = QGestureRecognizer::CancelGesture;
                   swipe->mSwipeState = MozSwipeGesture::CANCELLED;
               }
               if (swipe->mSwipeState == MozSwipeGesture::TRIGGERED) {
                   result = QGestureRecognizer::TriggerGesture;
               }
               else {
                   result = QGestureRecognizer::MayBeGesture;
               }
           }
           break;

        default:
            result = QGestureRecognizer::Ignore;
    }

    return result;
}

void
MozSwipeGestureRecognizer::reset(QGesture* aState)
{
    MozSwipeGesture* swipe = static_cast<MozSwipeGesture *>(aState);
    swipe->mHorizontalDirection = 0;
    swipe->mVerticalDirection = 0;
    QGestureRecognizer::reset(aState);
}

MozSwipeGesture::MozSwipeGesture()
  : mHorizontalDirection(0)
  , mVerticalDirection(0)
  , mSwipeState(MozSwipeGesture::NOT_STARTED)
{
}

int MozSwipeGesture::Direction()
{
    return mHorizontalDirection | mVerticalDirection;
}

bool
MozSwipeGesture::Update(const QTouchEvent::TouchPoint& aFirstPoint,
                        const QTouchEvent::TouchPoint& aSecondPoint,
                        const QSizeF& aSize)
{
    // Check that fingers are not too far away
    QPointF fingerDistance = aFirstPoint.pos() - aSecondPoint.pos();
    if (fingerDistance.manhattanLength() > MAX_FINGER_DISTANCE) {
        return false;
    }

    // Check that fingers doesn't move too much from the original distance
    QPointF startFingerDistance = aFirstPoint.startPos() - aSecondPoint.startPos();
    if ((startFingerDistance - fingerDistance).manhattanLength()
         > FINGER_DISTANCE_MISTAKE) {
        return false;
    }

    QPointF startPosition = aFirstPoint.startNormalizedPos();
    QPointF currentPosition = aFirstPoint.normalizedPos();

    float xDistance = fabs(currentPosition.x() - startPosition.x());
    float yDistance = fabs(currentPosition.y() - startPosition.y());

    startPosition = aFirstPoint.startPos();
    currentPosition = aFirstPoint.pos();

    if (!aSize.isEmpty()) {
        xDistance = fabs(currentPosition.x() - startPosition.x())
                    / aSize.width();
        yDistance = fabs(currentPosition.y() - startPosition.y())
                    / aSize.height();
    }

    mVerticalDirection = nsIDOMSimpleGestureEvent::DIRECTION_UP;
    if (currentPosition.y() > startPosition.y()) {
        mVerticalDirection = nsIDOMSimpleGestureEvent::DIRECTION_DOWN;
    }

    mHorizontalDirection = nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
    if (currentPosition.x() > startPosition.x()) {
        mHorizontalDirection = nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;
    }

    if (xDistance > TRIGGER_DISTANCE) {
        if (yDistance < TRIGGER_DISTANCE/2) {
            mVerticalDirection = 0;
        }
        mSwipeState = TRIGGERED;
    }

    if (yDistance > TRIGGER_DISTANCE) {
        if (xDistance < TRIGGER_DISTANCE/2) {
            mHorizontalDirection = 0;
        }
        mSwipeState = TRIGGERED;
    }

    // Use center of touchpoints as hotspot
    QPointF hotspot = aFirstPoint.pos() + aSecondPoint.pos();
    hotspot /= 2;
    setHotSpot(hotspot);
    return true;
}
