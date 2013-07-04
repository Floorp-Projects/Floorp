/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Timer;
import java.util.TimerTask;

import org.mozilla.gecko.util.GamepadUtils;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

public class ScrollAnimator implements View.OnGenericMotionListener {
    private Timer mScrollTimer;
    private int mX;
    private int mY;

    // Assuming 60fps, this will make the view scroll once per frame
    static final long MS_PER_FRAME = 1000 / 60;

    // Maximum number of pixels that can be scrolled per frame
    static final float MAX_SCROLL = 0.075f * GeckoAppShell.getDpi();

    private class ScrollRunnable extends TimerTask {
        private View mView;

        public ScrollRunnable(View view) {
            mView = view;
        }

        @Override
        public final void run() {
            mView.scrollBy(mX, mY);
        }
    }

    @Override
    public boolean onGenericMotion(View view, MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
            switch (event.getAction()) {
            case MotionEvent.ACTION_MOVE:
                // Cancel the animation if the joystick is in a neutral position
                if (GamepadUtils.isValueInDeadZone(event, MotionEvent.AXIS_X) &&
                        GamepadUtils.isValueInDeadZone(event, MotionEvent.AXIS_Y)) {
                    if (mScrollTimer != null) {
                        mScrollTimer.cancel();
                        mScrollTimer = null;
                    }
                    return true;
                }

                // Scroll with a velocity relative to the screen DPI
                mX = (int) (event.getAxisValue(MotionEvent.AXIS_X) * MAX_SCROLL);
                mY = (int) (event.getAxisValue(MotionEvent.AXIS_Y) * MAX_SCROLL);

                // Start the timer; the view will continue to scroll as long as
                // the joystick is not in the deadzone.
                if (mScrollTimer == null) {
                    mScrollTimer = new Timer();
                    ScrollRunnable task = new ScrollRunnable(view);
                    mScrollTimer.scheduleAtFixedRate(task, 0, MS_PER_FRAME);
                }

                return true;
            }
        }

        return false;
    }

    /**
     * Cancels the running scroll animation if it is in progress.
     */
    public void cancel() {
        if (mScrollTimer != null) {
            mScrollTimer.cancel();
            mScrollTimer = null;
        }
    }
}
