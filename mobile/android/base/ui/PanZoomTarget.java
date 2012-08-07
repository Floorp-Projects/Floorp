/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.ui;

import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;
import org.mozilla.gecko.gfx.ViewportMetrics;

import android.graphics.PointF;

public interface PanZoomTarget {
    public ImmutableViewportMetrics getViewportMetrics();
    public ZoomConstraints getZoomConstraints();

    public void setAnimationTarget(ViewportMetrics viewport);
    public void setViewportMetrics(ViewportMetrics viewport);
    public void scrollBy(PointF point);
    public void scaleWithFocus(float zoomFactor, PointF focus);

    public void notifyLayerClientOfGeometryChange();
    public void setForceRedraw();

    public boolean post(Runnable action);
    public Object getLock();
    public PointF convertViewPointToLayerPoint(PointF viewPoint);
}
