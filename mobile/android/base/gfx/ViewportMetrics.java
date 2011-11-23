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
import android.graphics.Rect;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerController;
import org.json.JSONException;
import org.json.JSONObject;
import android.util.Log;

/**
 * ViewportMetrics manages state and contains some utility functions related to
 * the page viewport for the Gecko layer client to use.
 */
public class ViewportMetrics {
    private IntSize mPageSize;
    private Rect mViewportRect;
    private Point mViewportOffset;

    public ViewportMetrics() {
        mPageSize = new IntSize(LayerController.TILE_WIDTH,
                                LayerController.TILE_HEIGHT);
        mViewportRect = new Rect(0, 0, 1, 1);
        mViewportOffset = new Point(0, 0);
    }

    public ViewportMetrics(ViewportMetrics viewport) {
        mPageSize = new IntSize(viewport.getPageSize());
        mViewportRect = new Rect(viewport.getViewport());
        mViewportOffset = new Point(viewport.getViewportOffset());
    }

    public ViewportMetrics(JSONObject json) throws JSONException {
        int x = json.getInt("x");
        int y = json.getInt("y");
        int width = json.getInt("width");
        int height = json.getInt("height");
        int pageWidth = json.getInt("pageWidth");
        int pageHeight = json.getInt("pageHeight");
        int offsetX = json.getInt("offsetX");
        int offsetY = json.getInt("offsetY");

        mPageSize = new IntSize(pageWidth, pageHeight);
        mViewportRect = new Rect(x, y, x + width, y + height);
        mViewportOffset = new Point(offsetX, offsetY);
    }

    public Point getOptimumViewportOffset() {
        // XXX We currently always position the viewport in the centre of the
        //     displayport, but we might want to optimise this during panning
        //     to minimise checkerboarding.
        Point optimumOffset =
            new Point((LayerController.TILE_WIDTH - mViewportRect.width()) / 2,
                      (LayerController.TILE_HEIGHT - mViewportRect.height()) / 2);

        /* XXX Until bug #524925 is fixed, changing the viewport origin will
         * probably cause things to be slower than just having a smaller usable
         * displayport.
         */
        Rect viewport = getClampedViewport();

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

        return optimumOffset;
    }

    public Point getOrigin() {
        return new Point(mViewportRect.left, mViewportRect.top);
    }

    public Point getDisplayportOrigin() {
        return new Point(mViewportRect.left - mViewportOffset.x,
                         mViewportRect.top - mViewportOffset.y);
    }

    public IntSize getSize() {
        return new IntSize(mViewportRect.width(), mViewportRect.height());
    }

    public Rect getViewport() {
        return mViewportRect;
    }

    /** Returns the viewport rectangle, clamped within the page-size. */
    public Rect getClampedViewport() {
        Rect clampedViewport = new Rect(mViewportRect);

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

    public Point getViewportOffset() {
        return mViewportOffset;
    }

    public IntSize getPageSize() {
        return mPageSize;
    }

    public void setPageSize(IntSize pageSize) {
        mPageSize = pageSize;
    }

    public void setViewport(Rect viewport) {
        mViewportRect = viewport;
    }

    public void setOrigin(Point origin) {
        mViewportRect.set(origin.x, origin.y,
                          origin.x + mViewportRect.width(),
                          origin.y + mViewportRect.height());
    }

    public void setSize(IntSize size) {
        mViewportRect.right = mViewportRect.left + size.width;
        mViewportRect.bottom = mViewportRect.top + size.height;
    }

    public void setViewportOffset(Point offset) {
        mViewportOffset = offset;
    }

    public boolean equals(ViewportMetrics viewport) {
        return mViewportRect.equals(viewport.getViewport()) &&
               mPageSize.equals(viewport.getPageSize()) &&
               mViewportOffset.equals(viewport.getViewportOffset());
    }

    public String toJSON() {
        return "{ \"x\" : " + mViewportRect.left +
               ", \"y\" : " + mViewportRect.top +
               ", \"width\" : " + mViewportRect.width() +
               ", \"height\" : " + mViewportRect.height() +
               ", \"pageWidth\" : " + mPageSize.width +
               ", \"pageHeight\" : " + mPageSize.height +
               ", \"offsetX\" : " + mViewportOffset.x +
               ", \"offsetY\" : " + mViewportOffset.y +
               "}";
    }
}

