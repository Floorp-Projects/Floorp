/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Chris Lord <chrislord.net@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.DisplayMetrics;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.RectUtils;
import org.json.JSONException;
import org.json.JSONObject;
import android.util.Log;

/**
 * ViewportMetrics manages state and contains some utility functions related to
 * the page viewport for the Gecko layer client to use.
 */
public class ViewportMetrics {
    private static final String LOGTAG = "GeckoViewportMetrics";

    private FloatSize mPageSize;
    private RectF mViewportRect;
    private PointF mViewportOffset;
    private float mZoomFactor;
    private boolean mAllowZoom;

    // A scale from -1,-1 to 1,1 that represents what edge of the displayport
    // we want the viewport to be biased towards.
    private PointF mViewportBias;
    private static final float MAX_BIAS = 0.8f;

    public ViewportMetrics() {
        DisplayMetrics metrics = new DisplayMetrics();
        GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

        mPageSize = new FloatSize(metrics.widthPixels, metrics.heightPixels);
        mViewportRect = new RectF(0, 0, metrics.widthPixels, metrics.heightPixels);
        mViewportOffset = new PointF(0, 0);
        mZoomFactor = 1.0f;
        mViewportBias = new PointF(0.0f, 0.0f);
        mAllowZoom = true;
    }

    public ViewportMetrics(ViewportMetrics viewport) {
        mPageSize = new FloatSize(viewport.getPageSize());
        mViewportRect = new RectF(viewport.getViewport());
        PointF offset = viewport.getViewportOffset();
        mViewportOffset = new PointF(offset.x, offset.y);
        mZoomFactor = viewport.getZoomFactor();
        mViewportBias = viewport.mViewportBias;
        mAllowZoom = viewport.mAllowZoom;
    }

    public ViewportMetrics(JSONObject json) throws JSONException {
        float x = (float)json.getDouble("x");
        float y = (float)json.getDouble("y");
        float width = (float)json.getDouble("width");
        float height = (float)json.getDouble("height");
        float pageWidth = (float)json.getDouble("pageWidth");
        float pageHeight = (float)json.getDouble("pageHeight");
        float offsetX = (float)json.getDouble("offsetX");
        float offsetY = (float)json.getDouble("offsetY");
        float zoom = (float)json.getDouble("zoom");

        mAllowZoom = json.getBoolean("allowZoom");

        mPageSize = new FloatSize(pageWidth, pageHeight);
        mViewportRect = new RectF(x, y, x + width, y + height);
        mViewportOffset = new PointF(offsetX, offsetY);
        mZoomFactor = zoom;
        mViewportBias = new PointF(0.0f, 0.0f);
    }

    public PointF getOptimumViewportOffset(IntSize displayportSize) {
        /* XXX Until bug #524925 is fixed, changing the viewport origin will
         *     cause unnecessary relayouts. This may cause rendering time to
         *     increase and should be considered.
         */
        RectF viewport = getClampedViewport();

        FloatSize bufferSpace = new FloatSize(displayportSize.width - viewport.width(),
                                            displayportSize.height - viewport.height());
        PointF optimumOffset =
            new PointF(bufferSpace.width * ((mViewportBias.x + 1.0f) / 2.0f),
                       bufferSpace.height * ((mViewportBias.y + 1.0f) / 2.0f));

        // Make sure this offset won't cause wasted pixels in the displayport
        // (i.e. make sure the resultant displayport intersects with the page
        //  as much as possible)
        if (viewport.left - optimumOffset.x < 0)
          optimumOffset.x = viewport.left;
        else if ((bufferSpace.width - optimumOffset.x) + viewport.right > mPageSize.width)
          optimumOffset.x = bufferSpace.width - (mPageSize.width - viewport.right);

        if (viewport.top - optimumOffset.y < 0)
          optimumOffset.y = viewport.top;
        else if ((bufferSpace.height - optimumOffset.y) + viewport.bottom > mPageSize.height)
          optimumOffset.y = bufferSpace.height - (mPageSize.height - viewport.bottom);

        return new PointF(Math.round(optimumOffset.x), Math.round(optimumOffset.y));
    }

    public PointF getOrigin() {
        return new PointF(mViewportRect.left, mViewportRect.top);
    }

    public PointF getDisplayportOrigin() {
        return new PointF(mViewportRect.left - mViewportOffset.x,
                          mViewportRect.top - mViewportOffset.y);
    }

    public FloatSize getSize() {
        return new FloatSize(mViewportRect.width(), mViewportRect.height());
    }

    public RectF getViewport() {
        return mViewportRect;
    }

    /** Returns the viewport rectangle, clamped within the page-size. */
    public RectF getClampedViewport() {
        RectF clampedViewport = new RectF(mViewportRect);

        // While the viewport size ought to never exceed the page size, we
        // do the clamping in this order to make sure that the origin is
        // never negative.
        if (clampedViewport.right > mPageSize.width)
            clampedViewport.offset(mPageSize.width - clampedViewport.right, 0);
        if (clampedViewport.left < 0)
            clampedViewport.offset(-clampedViewport.left, 0);

        if (clampedViewport.bottom > mPageSize.height)
            clampedViewport.offset(0, mPageSize.height - clampedViewport.bottom);
        if (clampedViewport.top < 0)
            clampedViewport.offset(0, -clampedViewport.top);

        return clampedViewport;
    }

    public PointF getViewportOffset() {
        return mViewportOffset;
    }

    public FloatSize getPageSize() {
        return mPageSize;
    }

    public float getZoomFactor() {
        return mZoomFactor;
    }

    public boolean getAllowZoom() {
        return mAllowZoom;
    }

    public void setPageSize(FloatSize pageSize) {
        mPageSize = pageSize;
    }

    public void setViewport(RectF viewport) {
        mViewportRect = viewport;
    }

    public void setOrigin(PointF origin) {
        // When the origin is set, we compare it with the last value set and
        // change the viewport bias accordingly, so that any viewport based
        // on these metrics will have a larger buffer in the direction of
        // movement.

        // XXX Note the comment about bug #524925 in getOptimumViewportOffset.
        //     Ideally, the viewport bias would be a sliding scale, but we
        //     don't want to change it too often at the moment.
        if (FloatUtils.fuzzyEquals(origin.x, mViewportRect.left))
            mViewportBias.x = 0;
        else
            mViewportBias.x = ((mViewportRect.left - origin.x) > 0) ? MAX_BIAS : -MAX_BIAS;
        if (FloatUtils.fuzzyEquals(origin.y, mViewportRect.top))
            mViewportBias.y = 0;
        else
            mViewportBias.y = ((mViewportRect.top - origin.y) > 0) ? MAX_BIAS : -MAX_BIAS;

        mViewportRect.set(origin.x, origin.y,
                          origin.x + mViewportRect.width(),
                          origin.y + mViewportRect.height());
    }

    public void setSize(FloatSize size) {
        mViewportRect.right = mViewportRect.left + size.width;
        mViewportRect.bottom = mViewportRect.top + size.height;
    }

    public void setViewportOffset(PointF offset) {
        mViewportOffset = offset;
    }

    public void setZoomFactor(float zoomFactor) {
        mZoomFactor = zoomFactor;
    }

    /* This will set the zoom factor and re-scale page-size and viewport offset
     * accordingly. The given focus will remain at the same point on the screen
     * after scaling.
     */
    public void scaleTo(float newZoomFactor, PointF focus) {
        float scaleFactor = newZoomFactor / mZoomFactor;

        mPageSize = mPageSize.scale(scaleFactor);

        PointF origin = getOrigin();
        origin.offset(focus.x, focus.y);
        origin = PointUtils.scale(origin, scaleFactor);
        origin.offset(-focus.x, -focus.y);
        setOrigin(origin);

        mZoomFactor = newZoomFactor;

        // Similar to setOrigin, set the viewport bias based on the focal point
        // of the zoom so that a viewport based on these metrics will have a
        // larger buffer based on the direction of movement when scaling.
        //
        // This is biased towards scaling outwards, as zooming in doesn't
        // really require a viewport bias.
        mViewportBias.set(((focus.x / mViewportRect.width()) * (2.0f * MAX_BIAS)) - MAX_BIAS,
                          ((focus.y / mViewportRect.height()) * (2.0f * MAX_BIAS)) - MAX_BIAS);
    }

    /*
     * Returns the viewport metrics that represent a linear transition between `from` and `to` at
     * time `t`, which is on the scale [0, 1). This function interpolates the viewport rect, the
     * page size, the offset, and the zoom factor.
     */
    public ViewportMetrics interpolate(ViewportMetrics to, float t) {
        ViewportMetrics result = new ViewportMetrics();
        result.mPageSize = mPageSize.interpolate(to.mPageSize, t);
        result.mZoomFactor = FloatUtils.interpolate(mZoomFactor, to.mZoomFactor, t);
        result.mViewportRect = RectUtils.interpolate(mViewportRect, to.mViewportRect, t);
        result.mViewportOffset = PointUtils.interpolate(mViewportOffset, to.mViewportOffset, t);
        return result;
    }

    public boolean fuzzyEquals(ViewportMetrics other) {
        return mPageSize.fuzzyEquals(other.mPageSize)
            && RectUtils.fuzzyEquals(mViewportRect, other.mViewportRect)
            && FloatUtils.fuzzyEquals(mViewportOffset, other.mViewportOffset)
            && FloatUtils.fuzzyEquals(mZoomFactor, other.mZoomFactor);
    }

    public String toJSON() {
        // Round off height and width. Since the height and width are the size of the screen, it
        // makes no sense to send non-integer coordinates to Gecko.
        int height = Math.round(mViewportRect.height());
        int width = Math.round(mViewportRect.width());

        StringBuffer sb = new StringBuffer(256);
        sb.append("{ \"x\" : ").append(mViewportRect.left)
          .append(", \"y\" : ").append(mViewportRect.top)
          .append(", \"width\" : ").append(width)
          .append(", \"height\" : ").append(height)
          .append(", \"pageWidth\" : ").append(mPageSize.width)
          .append(", \"pageHeight\" : ").append(mPageSize.height)
          .append(", \"offsetX\" : ").append(mViewportOffset.x)
          .append(", \"offsetY\" : ").append(mViewportOffset.y)
          .append(", \"zoom\" : ").append(mZoomFactor)
          .append(" }");
        return sb.toString();
    }

    @Override
    public String toString() {
        StringBuffer buff = new StringBuffer(128);
        buff.append("v=").append(mViewportRect.toString())
            .append(" p=").append(mPageSize.toString())
            .append(" z=").append(mZoomFactor)
            .append(" o=").append(mViewportOffset.x)
            .append(',').append(mViewportOffset.y);
        return buff.toString();
    }
}

