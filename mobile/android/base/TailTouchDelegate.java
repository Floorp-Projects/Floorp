/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;

/**
 * A TouchDelegate to pass the events from one view to another.
 * Usually it's better to give half of the tail's width to the other 
 * view that is being overlapped.
 */
public class TailTouchDelegate extends TouchDelegate {

    /**
     * Creates a TailTouchDelegate for a view.
     *
     * @param bounds        The rectangular bounds on the view which should delegate events.
     * @param delegateView  The view that should get the delegated events.
     */
    public TailTouchDelegate(Rect bounds, View delegateView) {
        super(bounds, delegateView);
    }

    @Override 
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                // Android bug 36445: Touch Delegation not reset on ACTION_DOWN.
                if (!super.onTouchEvent(event)) {
                    MotionEvent cancelEvent = MotionEvent.obtain(event);
                    cancelEvent.setAction(MotionEvent.ACTION_CANCEL);
                    super.onTouchEvent(cancelEvent);
                    return false;
                } else {
                    return true;
                }
            default:
                return super.onTouchEvent(event);
        }
    }
}
