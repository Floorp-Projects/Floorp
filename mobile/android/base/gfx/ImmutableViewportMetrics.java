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
    public final float pageRectLeft;
    public final float pageRectTop;
    public final float pageRectRight;
    public final float pageRectBottom;
    public final float cssPageRectLeft;
    public final float cssPageRectTop;
    public final float cssPageRectRight;
    public final float cssPageRectBottom;
    public final float viewportRectLeft;
    public final float viewportRectTop;
    public final float viewportRectRight;
    public final float viewportRectBottom;
    public final float zoomFactor;

    public ImmutableViewportMetrics(ViewportMetrics m) {
        RectF viewportRect = m.getViewport();
        viewportRectLeft = viewportRect.left;
        viewportRectTop = viewportRect.top;
        viewportRectRight = viewportRect.right;
        viewportRectBottom = viewportRect.bottom;

        RectF pageRect = m.getPageRect();
        pageRectLeft = pageRect.left;
        pageRectTop = pageRect.top;
        pageRectRight = pageRect.right;
        pageRectBottom = pageRect.bottom;

        RectF cssPageRect = m.getCssPageRect();
        cssPageRectLeft = cssPageRect.left;
        cssPageRectTop = cssPageRect.top;
        cssPageRectRight = cssPageRect.right;
        cssPageRectBottom = cssPageRect.bottom;

        zoomFactor = m.getZoomFactor();
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

    public RectF getCssViewport() {
        return RectUtils.scale(getViewport(), 1/zoomFactor);
    }

    public RectF getPageRect() {
        return new RectF(pageRectLeft, pageRectTop, pageRectRight, pageRectBottom);
    }

    public float getPageWidth() {
        return pageRectRight - pageRectLeft;
    }

    public float getPageHeight() {
        return pageRectBottom - pageRectTop;
    }

    public RectF getCssPageRect() {
        return new RectF(cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom);
    }

    @Override
    public String toString() {
        return "ImmutableViewportMetrics v=(" + viewportRectLeft + "," + viewportRectTop + ","
                + viewportRectRight + "," + viewportRectBottom + ") p=(" + pageRectLeft + ","
                + pageRectTop + "," + pageRectRight + "," + pageRectBottom + ") c=("
                + cssPageRectLeft + "," + cssPageRectTop + "," + cssPageRectRight + ","
                + cssPageRectBottom + ") z=" + zoomFactor;
    }
}
