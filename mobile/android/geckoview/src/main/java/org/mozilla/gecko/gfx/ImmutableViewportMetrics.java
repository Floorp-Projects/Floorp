/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.FloatUtils;

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
    public final int viewportRectWidth;
    public final int viewportRectHeight;

    public final float zoomFactor;
    public final boolean isRTL;

    public ImmutableViewportMetrics(DisplayMetrics metrics) {
        viewportRectLeft   = pageRectLeft   = cssPageRectLeft   = 0;
        viewportRectTop    = pageRectTop    = cssPageRectTop    = 0;
        viewportRectWidth = metrics.widthPixels;
        viewportRectHeight = metrics.heightPixels;
        pageRectRight  = cssPageRectRight  = metrics.widthPixels;
        pageRectBottom = cssPageRectBottom = metrics.heightPixels;
        zoomFactor = 1.0f;
        isRTL = false;
    }

    /** This constructor is used by native code in AndroidJavaWrappers.cpp, be
     * careful when modifying the signature.
     */
    @WrapForJNI(calledFrom = "gecko")
    public ImmutableViewportMetrics(float aPageRectLeft, float aPageRectTop,
        float aPageRectRight, float aPageRectBottom, float aCssPageRectLeft,
        float aCssPageRectTop, float aCssPageRectRight, float aCssPageRectBottom,
        float aViewportRectLeft, float aViewportRectTop, int aViewportRectWidth,
        int aViewportRectHeight, float aZoomFactor)
    {
        this(aPageRectLeft, aPageRectTop,
             aPageRectRight, aPageRectBottom, aCssPageRectLeft,
             aCssPageRectTop, aCssPageRectRight, aCssPageRectBottom,
             aViewportRectLeft, aViewportRectTop, aViewportRectWidth,
             aViewportRectHeight, aZoomFactor, false);
    }

    private ImmutableViewportMetrics(float aPageRectLeft, float aPageRectTop,
        float aPageRectRight, float aPageRectBottom, float aCssPageRectLeft,
        float aCssPageRectTop, float aCssPageRectRight, float aCssPageRectBottom,
        float aViewportRectLeft, float aViewportRectTop, int aViewportRectWidth,
        int aViewportRectHeight, float aZoomFactor, boolean aIsRTL)
    {
        pageRectLeft = aPageRectLeft;
        pageRectTop = aPageRectTop;
        pageRectRight = aPageRectRight;
        pageRectBottom = aPageRectBottom;
        cssPageRectLeft = aCssPageRectLeft;
        cssPageRectTop = aCssPageRectTop;
        cssPageRectRight = aCssPageRectRight;
        cssPageRectBottom = aCssPageRectBottom;
        viewportRectLeft = aViewportRectLeft;
        viewportRectTop = aViewportRectTop;
        viewportRectWidth = aViewportRectWidth;
        viewportRectHeight = aViewportRectHeight;
        zoomFactor = aZoomFactor;
        isRTL = aIsRTL;
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

    public RectF getCssViewport() {
        return RectUtils.scale(getViewport(), 1 / zoomFactor);
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

    public RectF getOverscroll() {
        return new RectF(Math.max(0, pageRectLeft - viewportRectLeft),
                         Math.max(0, pageRectTop - viewportRectTop),
                         Math.max(0, viewportRectRight() - pageRectRight),
                         Math.max(0, viewportRectBottom() - pageRectBottom));
    }

    /*
     * Returns the viewport metrics that represent a linear transition between "this" and "to" at
     * time "t", which is on the scale [0, 1). This function interpolates all values stored in
     * the viewport metrics.
     */
    public ImmutableViewportMetrics interpolate(ImmutableViewportMetrics to, float t) {
        return new ImmutableViewportMetrics(
            FloatUtils.interpolate(pageRectLeft, to.pageRectLeft, t),
            FloatUtils.interpolate(pageRectTop, to.pageRectTop, t),
            FloatUtils.interpolate(pageRectRight, to.pageRectRight, t),
            FloatUtils.interpolate(pageRectBottom, to.pageRectBottom, t),
            FloatUtils.interpolate(cssPageRectLeft, to.cssPageRectLeft, t),
            FloatUtils.interpolate(cssPageRectTop, to.cssPageRectTop, t),
            FloatUtils.interpolate(cssPageRectRight, to.cssPageRectRight, t),
            FloatUtils.interpolate(cssPageRectBottom, to.cssPageRectBottom, t),
            FloatUtils.interpolate(viewportRectLeft, to.viewportRectLeft, t),
            FloatUtils.interpolate(viewportRectTop, to.viewportRectTop, t),
            (int)FloatUtils.interpolate(viewportRectWidth, to.viewportRectWidth, t),
            (int)FloatUtils.interpolate(viewportRectHeight, to.viewportRectHeight, t),
            FloatUtils.interpolate(zoomFactor, to.zoomFactor, t),
            t >= 0.5 ? to.isRTL : isRTL);
    }

    public ImmutableViewportMetrics setViewportSize(int width, int height) {
        if (width == viewportRectWidth && height == viewportRectHeight) {
            return this;
        }

        return new ImmutableViewportMetrics(
            pageRectLeft, pageRectTop, pageRectRight, pageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            viewportRectLeft, viewportRectTop, width, height,
            zoomFactor, isRTL);
    }

    public ImmutableViewportMetrics setViewportOrigin(float newOriginX, float newOriginY) {
        return new ImmutableViewportMetrics(
            pageRectLeft, pageRectTop, pageRectRight, pageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            newOriginX, newOriginY, viewportRectWidth, viewportRectHeight,
            zoomFactor, isRTL);
    }

    public ImmutableViewportMetrics setZoomFactor(float newZoomFactor) {
        return new ImmutableViewportMetrics(
            pageRectLeft, pageRectTop, pageRectRight, pageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            viewportRectLeft, viewportRectTop, viewportRectWidth, viewportRectHeight,
            newZoomFactor, isRTL);
    }

    public ImmutableViewportMetrics offsetViewportBy(float dx, float dy) {
        return setViewportOrigin(viewportRectLeft + dx, viewportRectTop + dy);
    }

    public ImmutableViewportMetrics offsetViewportByAndClamp(float dx, float dy) {
        if (isRTL) {
            return setViewportOrigin(
                Math.min(pageRectRight - getWidth(), Math.max(viewportRectLeft + dx, pageRectLeft)),
                Math.max(pageRectTop, Math.min(viewportRectTop + dy, pageRectBottom - getHeight())));
        }
        return setViewportOrigin(
            Math.max(pageRectLeft, Math.min(viewportRectLeft + dx, pageRectRight - getWidth())),
            Math.max(pageRectTop, Math.min(viewportRectTop + dy, pageRectBottom - getHeight())));
    }

    public ImmutableViewportMetrics setPageRect(RectF pageRect, RectF cssPageRect) {
        return new ImmutableViewportMetrics(
            pageRect.left, pageRect.top, pageRect.right, pageRect.bottom,
            cssPageRect.left, cssPageRect.top, cssPageRect.right, cssPageRect.bottom,
            viewportRectLeft, viewportRectTop, viewportRectWidth, viewportRectHeight,
            zoomFactor, isRTL);
    }

    public ImmutableViewportMetrics setPageRectFrom(ImmutableViewportMetrics aMetrics) {
        if (aMetrics.cssPageRectLeft == cssPageRectLeft &&
            aMetrics.cssPageRectTop == cssPageRectTop &&
            aMetrics.cssPageRectRight == cssPageRectRight &&
            aMetrics.cssPageRectBottom == cssPageRectBottom) {
            return this;
        }
        RectF css = aMetrics.getCssPageRect();
        return setPageRect(RectUtils.scale(css, zoomFactor), css);
    }

    public ImmutableViewportMetrics setIsRTL(boolean aIsRTL) {
        if (isRTL == aIsRTL) {
            return this;
        }

        return new ImmutableViewportMetrics(
            pageRectLeft, pageRectTop, pageRectRight, pageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            viewportRectLeft, viewportRectTop, viewportRectWidth, viewportRectHeight,
            zoomFactor, aIsRTL);
    }

    /* This will set the zoom factor and re-scale page-size and viewport offset
     * accordingly. The given focus will remain at the same point on the screen
     * after scaling.
     */
    public ImmutableViewportMetrics scaleTo(float newZoomFactor, PointF focus) {
        // cssPageRect* is invariant, since we're setting the scale factor
        // here. The page rect is based on the CSS page rect.
        float newPageRectLeft = cssPageRectLeft * newZoomFactor;
        float newPageRectTop = cssPageRectTop * newZoomFactor;
        float newPageRectRight = cssPageRectLeft + ((cssPageRectRight - cssPageRectLeft) * newZoomFactor);
        float newPageRectBottom = cssPageRectTop + ((cssPageRectBottom - cssPageRectTop) * newZoomFactor);

        PointF origin = getOrigin();
        origin.offset(focus.x, focus.y);
        origin = PointUtils.scale(origin, newZoomFactor / zoomFactor);
        origin.offset(-focus.x, -focus.y);

        return new ImmutableViewportMetrics(
            newPageRectLeft, newPageRectTop, newPageRectRight, newPageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            origin.x, origin.y, viewportRectWidth, viewportRectHeight,
            newZoomFactor, isRTL);
    }

    /** Clamps the viewport to remain within the page rect. */
    public ImmutableViewportMetrics clamp() {
        RectF newViewport = getViewport();

        // The viewport bounds ought to never exceed the page bounds.
        if (newViewport.right > pageRectRight)
            newViewport.offset((pageRectRight) - newViewport.right, 0);
        if (newViewport.left < pageRectLeft)
            newViewport.offset(pageRectLeft - newViewport.left, 0);

        if (newViewport.bottom > pageRectBottom)
            newViewport.offset(0, (pageRectBottom) - newViewport.bottom);
        if (newViewport.top < pageRectTop)
            newViewport.offset(0, pageRectTop - newViewport.top);

        // Note that since newViewport is only translated around, the viewport's
        // width and height are unchanged.
        return new ImmutableViewportMetrics(
            pageRectLeft, pageRectTop, pageRectRight, pageRectBottom,
            cssPageRectLeft, cssPageRectTop, cssPageRectRight, cssPageRectBottom,
            newViewport.left, newViewport.top, viewportRectWidth, viewportRectHeight,
            zoomFactor, isRTL);
    }

    public boolean fuzzyEquals(ImmutableViewportMetrics other) {
        // Don't bother checking the pageRectXXX values because they are a product
        // of the cssPageRectXXX values and the zoomFactor, except with more rounding
        // error. Checking those is both inefficient and can lead to false negatives.
        return FloatUtils.fuzzyEquals(cssPageRectLeft, other.cssPageRectLeft)
            && FloatUtils.fuzzyEquals(cssPageRectTop, other.cssPageRectTop)
            && FloatUtils.fuzzyEquals(cssPageRectRight, other.cssPageRectRight)
            && FloatUtils.fuzzyEquals(cssPageRectBottom, other.cssPageRectBottom)
            && FloatUtils.fuzzyEquals(viewportRectLeft, other.viewportRectLeft)
            && FloatUtils.fuzzyEquals(viewportRectTop, other.viewportRectTop)
            && viewportRectWidth == other.viewportRectWidth
            && viewportRectHeight == other.viewportRectHeight
            && FloatUtils.fuzzyEquals(zoomFactor, other.zoomFactor);
    }

    @Override
    public String toString() {
        return "ImmutableViewportMetrics v=(" + viewportRectLeft + "," + viewportRectTop + ","
                + viewportRectWidth + "x" + viewportRectHeight + ") p=(" + pageRectLeft + ","
                + pageRectTop + "," + pageRectRight + "," + pageRectBottom + ") c=("
                + cssPageRectLeft + "," + cssPageRectTop + "," + cssPageRectRight + ","
                + cssPageRectBottom + ") z=" + zoomFactor + ", rtl=" + isRTL;
    }
}
