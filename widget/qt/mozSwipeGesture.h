/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSWIPEGESTURE_H
#define MOZSWIPEGESTURE_H

#include <QGestureRecognizer>
#include <QGesture>
#include <QTouchEvent>

class MozSwipeGestureRecognizer: public QGestureRecognizer
{
public:
    virtual QGesture* create(QObject* aTarget);
    virtual QGestureRecognizer::Result recognize(QGesture* aState,
                                                 QObject* aWatched,
                                                 QEvent* aEvent);
    virtual void reset(QGesture* aState);
};

class MozSwipeGesture: public QGesture
{
public:
    int Direction();

private:
    enum SwipeState {
        NOT_STARTED = 0,
        STARTED,
        CANCELLED,
        TRIGGERED
    };

    MozSwipeGesture();

    bool Update(const QTouchEvent::TouchPoint& aFirstPoint,
                const QTouchEvent::TouchPoint& aSecondPoint,
                const QSizeF& aSize);

    int mHorizontalDirection;
    int mVerticalDirection;
    SwipeState mSwipeState;
    friend class MozSwipeGestureRecognizer;
};

#endif
