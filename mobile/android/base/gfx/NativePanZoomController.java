/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.GeckoEventListener;

import org.json.JSONObject;

import android.graphics.PointF;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

class NativePanZoomController implements PanZoomController, GeckoEventListener {
    private final PanZoomTarget mTarget;
    private final EventDispatcher mDispatcher;
    private final CallbackRunnable mCallbackRunnable;

    NativePanZoomController(PanZoomTarget target, View view, EventDispatcher dispatcher) {
        mTarget = target;
        mDispatcher = dispatcher;
        mCallbackRunnable = new CallbackRunnable();
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            init();
        } else {
            mDispatcher.registerEventListener("Gecko:Ready", this);
        }
    }

    public void handleMessage(String event, JSONObject message) {
        if ("Gecko:Ready".equals(event)) {
            mDispatcher.unregisterEventListener("Gecko:Ready", this);
            init();
        }
    }

    public boolean onTouchEvent(MotionEvent event) {
        GeckoEvent wrapped = GeckoEvent.createMotionEvent(event, true);
        handleTouchEvent(wrapped);
        return false;
    }

    public boolean onMotionEvent(MotionEvent event) {
        // FIXME implement this
        return false;
    }

    public boolean onKeyEvent(KeyEvent event) {
        // FIXME implement this
        return false;
    }

    public PointF getVelocityVector() {
        // FIXME implement this
        return new PointF(0, 0);
    }

    public void pageRectUpdated() {
        // no-op in APZC, I think
    }

    public void abortPanning() {
        // no-op in APZC, I think
    }

    public native void abortAnimation();

    private native void init();
    private native void handleTouchEvent(GeckoEvent event);
    private native void handleMotionEvent(GeckoEvent event);
    private native long runDelayedCallback();

    public native void destroy();
    public native void notifyDefaultActionPrevented(boolean prevented);
    public native boolean getRedrawHint();
    public native void setOverScrollMode(int overscrollMode);
    public native int getOverScrollMode();

    /* Invoked from JNI */
    private void requestContentRepaint(float x, float y, float width, float height, float resolution) {
        mTarget.forceRedraw(new DisplayPortMetrics(x, y, x + width, y + height, resolution));
    }

    /* Invoked from JNI */
    private void postDelayedCallback(long delay) {
        mTarget.postDelayed(mCallbackRunnable, delay);
    }

    class CallbackRunnable implements Runnable {
        @Override
        public void run() {
            long nextDelay = runDelayedCallback();
            if (nextDelay >= 0) {
                mTarget.postDelayed(this, nextDelay);
            }
        }
    }

    public native void updateScrollOffset(float cssX, float cssY);
}
