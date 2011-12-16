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
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.RectUtils;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONStringer;
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

    public ViewportMetrics() {
        mPageSize = new FloatSize(1, 1);
        mViewportRect = new RectF(0, 0, 1, 1);
        mViewportOffset = new PointF(0, 0);
        mZoomFactor = 1.0f;
    }

    public ViewportMetrics(ViewportMetrics viewport) {
        mPageSize = new FloatSize(viewport.getPageSize());
        mViewportRect = new RectF(viewport.getViewport());
        PointF offset = viewport.getViewportOffset();
        mViewportOffset = new PointF(offset.x, offset.y);
        mZoomFactor = viewport.getZoomFactor();
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

        mPageSize = new FloatSize(pageWidth, pageHeight);
        mViewportRect = new RectF(x, y, x + width, y + height);
        mViewportOffset = new PointF(offsetX, offsetY);
        mZoomFactor = zoom;
    }

    public PointF getOptimumViewportOffset(IntSize displayportSize) {
        // XXX We currently always position the viewport in the centre of the
        //     displayport, but we might want to optimise this during panning
        //     to minimise checkerboarding.
        Point optimumOffset =
            new Point((int)Math.round((displayportSize.width - mViewportRect.width()) / 2),
                      (int)Math.round((displayportSize.height - mViewportRect.height()) / 2));

        /* XXX Until bug #524925 is fixed, changing the viewport origin will
         * probably cause things to be slower than just having a smaller usable
         * displayport.
         */
        Rect viewport = RectUtils.round(getClampedViewport());

        // Make sure this offset won't cause wasted pixels in the displayport
        // (i.e. make sure the resultant displayport intersects with the page
        //  as much as possible)
        if (viewport.left - optimumOffset.x < 0)
          optimumOffset.x = viewport.left;
        else if (optimumOffset.x + viewport.right > mPageSize.width)
          optimumOffset.x -= (mPageSize.width - (optimumOffset.x + viewport.right));

        if (viewport.top - optimumOffset.y < 0)
          optimumOffset.y = viewport.top;
        else if (optimumOffset.y + viewport.bottom > mPageSize.height)
          optimumOffset.y -= (mPageSize.height - (optimumOffset.y + viewport.bottom));

        return new PointF(optimumOffset);
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

    public void setPageSize(FloatSize pageSize) {
        mPageSize = pageSize;
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

    public String toJSON() {
        try {
            JSONStringer object = new JSONStringer().object();
            object.key("zoom").value(mZoomFactor);
            object.key("offsetY").value(mViewportOffset.y);
            object.key("offsetX").value(mViewportOffset.x);
            object.key("pageHeight").value(mPageSize.height);
            object.key("pageWidth").value(mPageSize.width);
            object.key("height").value(mViewportRect.height());
            object.key("width").value(mViewportRect.width());
            object.key("y").value(mViewportRect.top);
            object.key("x").value(mViewportRect.left);
            return object.endObject().toString();
        } catch (JSONException je) {
            Log.e(LOGTAG, "Error serializing viewportmetrics", je);
            return "";
        }
    }
}

