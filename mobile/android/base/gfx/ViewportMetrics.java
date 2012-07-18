/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.PointF;
import android.graphics.RectF;
import android.util.DisplayMetrics;

import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoApp;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * ViewportMetrics manages state and contains some utility functions related to
 * the page viewport for the Gecko layer client to use.
 */
public class ViewportMetrics {
    private static final String LOGTAG = "GeckoViewportMetrics";

    private RectF mPageRect;
    private RectF mCssPageRect;
    private RectF mViewportRect;
    private float mZoomFactor;

    public ViewportMetrics() {
        DisplayMetrics metrics = GeckoApp.mAppContext.getDisplayMetrics();

        mPageRect = new RectF(0, 0, metrics.widthPixels, metrics.heightPixels);
        mCssPageRect = new RectF(0, 0, metrics.widthPixels, metrics.heightPixels);
        mViewportRect = new RectF(0, 0, metrics.widthPixels, metrics.heightPixels);
        mZoomFactor = 1.0f;
    }

    public ViewportMetrics(ViewportMetrics viewport) {
        mPageRect = new RectF(viewport.getPageRect());
        mCssPageRect = new RectF(viewport.getCssPageRect());
        mViewportRect = new RectF(viewport.getViewport());
        mZoomFactor = viewport.getZoomFactor();
    }

    public ViewportMetrics(ImmutableViewportMetrics viewport) {
        mPageRect = new RectF(viewport.pageRectLeft,
                viewport.pageRectTop,
                viewport.pageRectRight,
                viewport.pageRectBottom);
        mCssPageRect = new RectF(viewport.cssPageRectLeft,
                viewport.cssPageRectTop,
                viewport.cssPageRectRight,
                viewport.cssPageRectBottom);
        mViewportRect = new RectF(viewport.viewportRectLeft,
                viewport.viewportRectTop,
                viewport.viewportRectRight,
                viewport.viewportRectBottom);
        mZoomFactor = viewport.zoomFactor;
    }


    public ViewportMetrics(JSONObject json) throws JSONException {
        float x = (float)json.getDouble("x");
        float y = (float)json.getDouble("y");
        float width = (float)json.getDouble("width");
        float height = (float)json.getDouble("height");
        float pageLeft = (float)json.getDouble("pageLeft");
        float pageTop = (float)json.getDouble("pageTop");
        float pageRight = (float)json.getDouble("pageRight");
        float pageBottom = (float)json.getDouble("pageBottom");
        float cssPageLeft = (float)json.getDouble("cssPageLeft");
        float cssPageTop = (float)json.getDouble("cssPageTop");
        float cssPageRight = (float)json.getDouble("cssPageRight");
        float cssPageBottom = (float)json.getDouble("cssPageBottom");
        float zoom = (float)json.getDouble("zoom");

        mPageRect = new RectF(pageLeft, pageTop, pageRight, pageBottom);
        mCssPageRect = new RectF(cssPageLeft, cssPageTop, cssPageRight, cssPageBottom);
        mViewportRect = new RectF(x, y, x + width, y + height);
        mZoomFactor = zoom;
    }

    public PointF getOrigin() {
        return new PointF(mViewportRect.left, mViewportRect.top);
    }

    public FloatSize getSize() {
        return new FloatSize(mViewportRect.width(), mViewportRect.height());
    }

    public RectF getViewport() {
        return mViewportRect;
    }

    public RectF getCssViewport() {
        return RectUtils.scale(mViewportRect, 1/mZoomFactor);
    }

    /** Returns the viewport rectangle, clamped within the page-size. */
    public RectF getClampedViewport() {
        RectF clampedViewport = new RectF(mViewportRect);

        // The viewport bounds ought to never exceed the page bounds.
        if (clampedViewport.right > mPageRect.right)
            clampedViewport.offset(mPageRect.right - clampedViewport.right, 0);
        if (clampedViewport.left < mPageRect.left)
            clampedViewport.offset(mPageRect.left - clampedViewport.left, 0);

        if (clampedViewport.bottom > mPageRect.bottom)
            clampedViewport.offset(0, mPageRect.bottom - clampedViewport.bottom);
        if (clampedViewport.top < mPageRect.top)
            clampedViewport.offset(0, mPageRect.top - clampedViewport.top);

        return clampedViewport;
    }

    public RectF getPageRect() {
        return mPageRect;
    }

    public RectF getCssPageRect() {
        return mCssPageRect;
    }

    public float getZoomFactor() {
        return mZoomFactor;
    }

    public void setPageRect(RectF pageRect, RectF cssPageRect) {
        mPageRect = pageRect;
        mCssPageRect = cssPageRect;
    }

    public void setViewport(RectF viewport) {
        mViewportRect = viewport;
    }

    public void setOrigin(PointF origin) {
        mViewportRect.set(origin.x, origin.y,
                          origin.x + mViewportRect.width(),
                          origin.y + mViewportRect.height());
    }

    public void setSize(FloatSize size) {
        mViewportRect.right = mViewportRect.left + size.width;
        mViewportRect.bottom = mViewportRect.top + size.height;
    }

    public void setZoomFactor(float zoomFactor) {
        mZoomFactor = zoomFactor;
    }

    /* This will set the zoom factor and re-scale page-size and viewport offset
     * accordingly. The given focus will remain at the same point on the screen
     * after scaling.
     */
    public void scaleTo(float newZoomFactor, PointF focus) {
        // mCssPageRect is invariant, since we're setting the scale factor
        // here. The page rect is based on the CSS page rect.
        mPageRect = RectUtils.scale(mCssPageRect, newZoomFactor);

        float scaleFactor = newZoomFactor / mZoomFactor;
        PointF origin = getOrigin();

        origin.offset(focus.x, focus.y);
        origin = PointUtils.scale(origin, scaleFactor);
        origin.offset(-focus.x, -focus.y);

        setOrigin(origin);

        mZoomFactor = newZoomFactor;
    }

    /*
     * Returns the viewport metrics that represent a linear transition between `from` and `to` at
     * time `t`, which is on the scale [0, 1). This function interpolates the viewport rect, the
     * page size, the offset, and the zoom factor.
     */
    public ViewportMetrics interpolate(ViewportMetrics to, float t) {
        ViewportMetrics result = new ViewportMetrics();
        result.mPageRect = RectUtils.interpolate(mPageRect, to.mPageRect, t);
        result.mCssPageRect = RectUtils.interpolate(mCssPageRect, to.mCssPageRect, t);
        result.mZoomFactor = FloatUtils.interpolate(mZoomFactor, to.mZoomFactor, t);
        result.mViewportRect = RectUtils.interpolate(mViewportRect, to.mViewportRect, t);
        return result;
    }

    public boolean fuzzyEquals(ViewportMetrics other) {
        return RectUtils.fuzzyEquals(mPageRect, other.mPageRect)
            && RectUtils.fuzzyEquals(mCssPageRect, other.mCssPageRect)
            && RectUtils.fuzzyEquals(mViewportRect, other.mViewportRect)
            && FloatUtils.fuzzyEquals(mZoomFactor, other.mZoomFactor);
    }

    public String toJSON() {
        // Round off height and width. Since the height and width are the size of the screen, it
        // makes no sense to send non-integer coordinates to Gecko.
        int height = Math.round(mViewportRect.height());
        int width = Math.round(mViewportRect.width());

        StringBuffer sb = new StringBuffer(512);
        sb.append("{ \"x\" : ").append(mViewportRect.left)
          .append(", \"y\" : ").append(mViewportRect.top)
          .append(", \"width\" : ").append(width)
          .append(", \"height\" : ").append(height)
          .append(", \"pageLeft\" : ").append(mPageRect.left)
          .append(", \"pageTop\" : ").append(mPageRect.top)
          .append(", \"pageRight\" : ").append(mPageRect.right)
          .append(", \"pageBottom\" : ").append(mPageRect.bottom)
          .append(", \"cssPageLeft\" : ").append(mCssPageRect.left)
          .append(", \"cssPageTop\" : ").append(mCssPageRect.top)
          .append(", \"cssPageRight\" : ").append(mCssPageRect.right)
          .append(", \"cssPageBottom\" : ").append(mCssPageRect.bottom)
          .append(", \"zoom\" : ").append(mZoomFactor)
          .append(" }");
        return sb.toString();
    }

    @Override
    public String toString() {
        StringBuffer buff = new StringBuffer(256);
        buff.append("v=").append(mViewportRect.toString())
            .append(" p=").append(mPageRect.toString())
            .append(" c=").append(mCssPageRect.toString())
            .append(" z=").append(mZoomFactor);
        return buff.toString();
    }
}

