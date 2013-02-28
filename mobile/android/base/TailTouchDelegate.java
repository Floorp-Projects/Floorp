/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.graphics.Rect;
import android.view.TouchDelegate;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * A TouchDelegate to pass the events from one view to another.
 * Usually it's better to give half of the tail's width to the other 
 * view that is being overlapped.
 */
public class TailTouchDelegate extends TouchDelegate {
    // Actual delegate that got the ACTION_DOWN event.
    private TouchDelegate mDelegate;

    // List of delegates.
    private List<TouchDelegate> mDelegates;

    /**
     * Creates an empty TailTouchDelegate for a view.
     *
     * @param bounds        The rectangular bounds on the view which should delegate events.
     * @param delegateView  The view that should get the delegated events.
     */
    public TailTouchDelegate(Rect bounds, View delegateView) {
        super(bounds, delegateView);
        mDelegates = new ArrayList<TouchDelegate>();
        mDelegates.add(new TouchDelegate(bounds, delegateView));
    }

    /**
     * Adds an Android TouchDelegate for the view.
     *
     * @param bounds        The rectangular bounds on the view which should delegate events.
     * @param delegateView  The view that should get the delegated events.
     */
    public void add(Rect bounds, View delegateView) {
        mDelegates.add(new TouchDelegate(bounds, delegateView));
    }

    @Override 
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                // Android bug 36445: Touch Delegation not reset on ACTION_DOWN.
                for (TouchDelegate delegate : mDelegates) {
                    if (delegate.onTouchEvent(event)) {
                        mDelegate = delegate;
                        return true;
                    }

                    MotionEvent cancelEvent = MotionEvent.obtain(event);
                    cancelEvent.setAction(MotionEvent.ACTION_CANCEL);
                    delegate.onTouchEvent(cancelEvent);
                    mDelegate = null;
                }
                return false;
            default:
                if (mDelegate != null)
                    return mDelegate.onTouchEvent(event);
                else
                    return false;
        }
    }
}
