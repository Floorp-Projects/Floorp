/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.EventDispatcher;

import android.graphics.PointF;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

public interface PanZoomController {
    // Threshold for sending touch move events to content
    public static final float CLICK_THRESHOLD = 1 / 50f * GeckoAppShell.getDpi();

    static class Factory {
        static PanZoomController create(PanZoomTarget target, View view, EventDispatcher dispatcher) {
            return new NativePanZoomController(target, view);
        }
    }

    public void destroy();
    public void attach();

    public boolean onTouchEvent(MotionEvent event);
    public boolean onMotionEvent(MotionEvent event);
    public void onMotionEventVelocity(final long aEventTime, final float aSpeedY);

    public void setOverscrollHandler(final Overscroll controller);

    public void setIsLongpressEnabled(boolean isLongpressEnabled);

    public ImmutableViewportMetrics adjustScrollForSurfaceShift(ImmutableViewportMetrics aMetrics, PointF aShift);
}
