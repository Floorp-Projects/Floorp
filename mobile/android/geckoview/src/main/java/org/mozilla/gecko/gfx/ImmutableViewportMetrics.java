/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.PointF;
import android.graphics.RectF;
import android.util.DisplayMetrics;

/**
 * ImmutableViewportMetrics are used to store the viewport metrics
 * in way that we can access a version of them from multiple threads
 * without having to take a lock
 */
public class ImmutableViewportMetrics {

    // We need to flatten the RectF and FloatSize structures
    // because Java doesn't have the concept of const classes
    public final float viewportRectLeft;
    public final float viewportRectTop;
    public final int viewportRectWidth;
    public final int viewportRectHeight;

    public final float zoomFactor;

    public ImmutableViewportMetrics(DisplayMetrics metrics) {
        viewportRectLeft   = 0;
        viewportRectTop    = 0;
        viewportRectWidth = metrics.widthPixels;
        viewportRectHeight = metrics.heightPixels;
        zoomFactor = 1.0f;
    }

    private ImmutableViewportMetrics(
        float aViewportRectLeft, float aViewportRectTop, int aViewportRectWidth,
        int aViewportRectHeight, float aZoomFactor)
    {
        viewportRectLeft = aViewportRectLeft;
        viewportRectTop = aViewportRectTop;
        viewportRectWidth = aViewportRectWidth;
        viewportRectHeight = aViewportRectHeight;
        zoomFactor = aZoomFactor;
    }

    public float getWidth() {
        return viewportRectWidth;
    }

    public float getHeight() {
        return viewportRectHeight;
    }

    public float viewportRectRight() {
        return viewportRectLeft + viewportRectWidth;
    }

    public float viewportRectBottom() {
        return viewportRectTop + viewportRectHeight;
    }

    public PointF getOrigin() {
        return new PointF(viewportRectLeft, viewportRectTop);
    }

    public FloatSize getSize() {
        return new FloatSize(viewportRectWidth, viewportRectHeight);
    }

    public RectF getViewport() {
        return new RectF(viewportRectLeft,
                         viewportRectTop,
                         viewportRectRight(),
                         viewportRectBottom());
    }

    public ImmutableViewportMetrics setViewportSize(int width, int height) {
        if (width == viewportRectWidth && height == viewportRectHeight) {
            return this;
        }

        return new ImmutableViewportMetrics(
            viewportRectLeft, viewportRectTop, width, height,
            zoomFactor);
    }

    public ImmutableViewportMetrics setViewportOrigin(float newOriginX, float newOriginY) {
        return new ImmutableViewportMetrics(
            newOriginX, newOriginY, viewportRectWidth, viewportRectHeight,
            zoomFactor);
    }

    public ImmutableViewportMetrics setZoomFactor(float newZoomFactor) {
        return new ImmutableViewportMetrics(
            viewportRectLeft, viewportRectTop, viewportRectWidth, viewportRectHeight,
            newZoomFactor);
    }

    @Override
    public String toString() {
        return "ImmutableViewportMetrics v=(" + viewportRectLeft + "," + viewportRectTop + ","
                + viewportRectWidth + "x" + viewportRectHeight + ") z=" + zoomFactor;
    }
}
