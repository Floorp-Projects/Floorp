/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.ZoomConstraints;

import android.graphics.PointF;

public interface PanZoomTarget {
    public ImmutableViewportMetrics getViewportMetrics();
    public ZoomConstraints getZoomConstraints();
    public boolean isFullScreen();

    public void setAnimationTarget(ImmutableViewportMetrics viewport);
    public void setViewportMetrics(ImmutableViewportMetrics viewport);
    public void scrollBy(float dx, float dy);
    public void panZoomStopped();
    /** This triggers an (asynchronous) viewport update/redraw. */
    public void forceRedraw();

    public boolean post(Runnable action);
    public Object getLock();
    public PointF convertViewPointToLayerPoint(PointF viewPoint);
}
