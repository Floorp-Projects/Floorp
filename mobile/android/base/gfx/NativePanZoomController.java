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
    private final EventDispatcher mDispatcher;

    NativePanZoomController(PanZoomTarget target, View view, EventDispatcher dispatcher) {
        mDispatcher = dispatcher;
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

    public void abortAnimation() {
        // no-op in APZC, I think
    }

    private native void init();
    private native void handleTouchEvent(GeckoEvent event);
    private native void handleMotionEvent(GeckoEvent event);

    public native void destroy();
    public native void notifyDefaultActionPrevented(boolean prevented);
    public native boolean getRedrawHint();
    public native void setOverScrollMode(int overscrollMode);
    public native int getOverScrollMode();
}
