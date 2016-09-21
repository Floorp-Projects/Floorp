/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewConfiguration;

/**
 * This is a copy of TouchDelegate from
 * https://github.com/android/platform_frameworks_base/blob/4b1a8f46d6ec55796bf77fd8921a5a242a219278/core/java/android/view/TouchDelegate.java
 * with a fix to reset mDelegateTargeted on each new gesture - the sole substantive change is a new
 * else leg in the ACTION_DOWN case of onTouchEvent marked by "START|END BUG FIX" comments.
 */

/**
 * Helper class to handle situations where you want a view to have a larger touch area than its
 * actual view bounds. The view whose touch area is changed is called the delegate view. This
 * class should be used by an ancestor of the delegate. To use a TouchDelegate, first create an
 * instance that specifies the bounds that should be mapped to the delegate and the delegate
 * view itself.
 * <p>
 * The ancestor should then forward all of its touch events received in its
 * {@link android.view.View#onTouchEvent(MotionEvent)} to {@link #onTouchEvent(MotionEvent)}.
 * </p>
 */
public class TouchDelegateWithReset extends TouchDelegate {

    /**
     * View that should receive forwarded touch events
     */
    private View mDelegateView;

    /**
     * Bounds in local coordinates of the containing view that should be mapped to the delegate
     * view. This rect is used for initial hit testing.
     */
    private Rect mBounds;

    /**
     * mBounds inflated to include some slop. This rect is to track whether the motion events
     * should be considered to be be within the delegate view.
     */
    private Rect mSlopBounds;

    /**
     * True if the delegate had been targeted on a down event (intersected mBounds).
     */
    private boolean mDelegateTargeted;

    private int mSlop;

    /**
     * Constructor
     *
     * @param bounds Bounds in local coordinates of the containing view that should be mapped to
     *        the delegate view
     * @param delegateView The view that should receive motion events
     */
    public TouchDelegateWithReset(Rect bounds, View delegateView) {
        super(bounds, delegateView);

        mBounds = bounds;

        mSlop = ViewConfiguration.get(delegateView.getContext()).getScaledTouchSlop();
        mSlopBounds = new Rect(bounds);
        mSlopBounds.inset(-mSlop, -mSlop);
        mDelegateView = delegateView;
    }

    /**
     * Will forward touch events to the delegate view if the event is within the bounds
     * specified in the constructor.
     *
     * @param event The touch event to forward
     * @return True if the event was forwarded to the delegate, false otherwise.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int x = (int)event.getX();
        int y = (int)event.getY();
        boolean sendToDelegate = false;
        boolean hit = true;
        boolean handled = false;

        switch (event.getAction()) {
        case MotionEvent.ACTION_DOWN:
            Rect bounds = mBounds;

            if (bounds.contains(x, y)) {
                mDelegateTargeted = true;
                sendToDelegate = true;
            } /* START BUG FIX */
            else {
                mDelegateTargeted = false;
            }
            /* END BUG FIX */
            break;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_MOVE:
            sendToDelegate = mDelegateTargeted;
            if (sendToDelegate) {
                Rect slopBounds = mSlopBounds;
                if (!slopBounds.contains(x, y)) {
                    hit = false;
                }
            }
            break;
        case MotionEvent.ACTION_CANCEL:
            sendToDelegate = mDelegateTargeted;
            mDelegateTargeted = false;
            break;
        }
        if (sendToDelegate) {
            final View delegateView = mDelegateView;

            if (hit) {
                // Offset event coordinates to be inside the target view
                event.setLocation(delegateView.getWidth() / 2, delegateView.getHeight() / 2);
            } else {
                // Offset event coordinates to be outside the target view (in case it does
                // something like tracking pressed state)
                int slop = mSlop;
                event.setLocation(-(slop * 2), -(slop * 2));
            }
            handled = delegateView.dispatchTouchEvent(event);
        }
        return handled;
    }
}