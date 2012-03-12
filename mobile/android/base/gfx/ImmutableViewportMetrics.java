/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.PointF;
import android.graphics.RectF;

/**
 * ImmutableViewportMetrics are used to store the viewport metrics
 * in way that we can access a version of them from multiple threads
 * without having to take a lock
 */
public class ImmutableViewportMetrics {

    // We need to flatten the RectF and FloatSize structures
    // because Java doesn't have the concept of const classes
    public final float pageSizeWidth;
    public final float pageSizeHeight;
    public final float viewportRectBottom;
    public final float viewportRectLeft;
    public final float viewportRectRight;
    public final float viewportRectTop;
    public final float zoomFactor;
    public final boolean allowZoom;

    public ImmutableViewportMetrics(ViewportMetrics m) {
        RectF viewportRect = m.getViewport();
        viewportRectBottom = viewportRect.bottom;
        viewportRectLeft = viewportRect.left;
        viewportRectRight = viewportRect.right;
        viewportRectTop = viewportRect.top;

        FloatSize pageSize = m.getPageSize();
        pageSizeWidth = pageSize.width;
        pageSizeHeight = pageSize.height;

        zoomFactor = m.getZoomFactor();
        allowZoom = m.getAllowZoom();
    }

    public float getWidth() {
        return viewportRectRight - viewportRectLeft;
    }

    public float getHeight() {
        return viewportRectBottom - viewportRectTop;
    }

    // some helpers to make ImmutableViewportMetrics act more like ViewportMetrics

    public PointF getOrigin() {
        return new PointF(viewportRectLeft, viewportRectTop);
    }

    public FloatSize getSize() {
        return new FloatSize(viewportRectRight - viewportRectLeft, viewportRectBottom - viewportRectTop);
    }

    public RectF getViewport() {
        return new RectF(viewportRectLeft,
                         viewportRectTop,
                         viewportRectRight,
                         viewportRectBottom);
    }

    public FloatSize getPageSize() {
        return new FloatSize(pageSizeWidth, pageSizeHeight);
    }

    public boolean getAllowZoom() {
        return allowZoom;
    }
}
