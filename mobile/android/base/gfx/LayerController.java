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

import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerClient;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.GeckoApp;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.view.MotionEvent;
import android.view.GestureDetector;
import android.view.ScaleGestureDetector;
import android.view.View.OnTouchListener;
import java.lang.Math;
import java.util.ArrayList;

/**
 * The layer controller manages a tile that represents the visible page. It does panning and
 * zooming natively by delegating to a panning/zooming controller. Touch events can be dispatched
 * to a higher-level view.
 */
public class LayerController {
    private static final String LOGTAG = "GeckoLayerController";

    private Layer mRootLayer;                   /* The root layer. */
    private LayerView mView;                    /* The main rendering view. */
    private Context mContext;                   /* The current context. */
    private ViewportMetrics mViewportMetrics;   /* The current viewport metrics. */

    private PanZoomController mPanZoomController;
    /*
     * The panning and zooming controller, which interprets pan and zoom gestures for us and
     * updates our visible rect appropriately.
     */

    private OnTouchListener mOnTouchListener;   /* The touch listener. */
    private LayerClient mLayerClient;           /* The layer client. */

    private boolean mForceRedraw;

    /* NB: These must be powers of two due to the OpenGL ES 1.x restriction on NPOT textures. */
    public static final int TILE_WIDTH = 1024;
    public static final int TILE_HEIGHT = 2048;

    /* If the visible rect is within the danger zone (measured in pixels from each edge of a tile),
     * we start aggressively redrawing to minimize checkerboarding. */
    private static final int DANGER_ZONE_X = 75;
    private static final int DANGER_ZONE_Y = 150;

    public LayerController(Context context) {
        mContext = context;

        mForceRedraw = true;
        mViewportMetrics = new ViewportMetrics();
        mPanZoomController = new PanZoomController(this);
        mView = new LayerView(context, this);
    }

    public void setRoot(Layer layer) { mRootLayer = layer; }

    public void setLayerClient(LayerClient layerClient) {
        mLayerClient = layerClient;
        layerClient.setLayerController(this);
    }

    public void setForceRedraw() {
        mForceRedraw = true;
    }

    public LayerClient getLayerClient()           { return mLayerClient; }
    public Layer getRoot()                        { return mRootLayer; }
    public LayerView getView()                    { return mView; }
    public Context getContext()                   { return mContext; }
    public ViewportMetrics getViewportMetrics()   { return mViewportMetrics; }

    public RectF getViewport() {
        return mViewportMetrics.getViewport();
    }

    public FloatSize getViewportSize() {
        return mViewportMetrics.getSize();
    }

    public FloatSize getPageSize() {
        return mViewportMetrics.getPageSize();
    }

    public PointF getOrigin() {
        return mViewportMetrics.getOrigin();
    }

    public float getZoomFactor() {
        return mViewportMetrics.getZoomFactor();
    }

    public Bitmap getCheckerboardPattern()  { return getDrawable("checkerboard"); }
    public Bitmap getShadowPattern()        { return getDrawable("shadow"); }

    public GestureDetector.OnGestureListener getGestureListener()                   { return mPanZoomController; }
    public ScaleGestureDetector.OnScaleGestureListener getScaleGestureListener()    { return mPanZoomController; }
    public GestureDetector.OnDoubleTapListener getDoubleTapListener()               { return mPanZoomController; }

    private Bitmap getDrawable(String name) {
        Resources resources = mContext.getResources();
        int resourceID = resources.getIdentifier(name, "drawable", mContext.getPackageName());
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        return BitmapFactory.decodeResource(mContext.getResources(), resourceID, options);
    }

    /**
     * The view calls this to indicate that the viewport changed size.
     *
     * TODO: Refactor this to use an interface. Expose that interface only to the view and not
     * to the layer client. That way, the layer client won't be tempted to call this, which might
     * result in an infinite loop.
     */
    public void setViewportSize(FloatSize size) {
        mViewportMetrics.setSize(size);
        setForceRedraw();

        if (mLayerClient != null)
            mLayerClient.viewportSizeChanged();

        notifyLayerClientOfGeometryChange();
        mPanZoomController.geometryChanged(true);
        mView.requestRender();
    }

    public void scrollTo(PointF point) {
        mViewportMetrics.setOrigin(point);
        notifyLayerClientOfGeometryChange();
        mPanZoomController.geometryChanged(false);
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    public void scrollBy(PointF point) {
        PointF origin = mViewportMetrics.getOrigin();
        origin.offset(point.x, point.y);
        mViewportMetrics.setOrigin(origin);

        notifyLayerClientOfGeometryChange();
        mPanZoomController.geometryChanged(false);
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    public void setViewport(RectF viewport) {
        mViewportMetrics.setViewport(viewport);
        notifyLayerClientOfGeometryChange();
        mPanZoomController.geometryChanged(false);
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    public void setPageSize(FloatSize size) {
        if (mViewportMetrics.getPageSize().fuzzyEquals(size))
            return;

        mViewportMetrics.setPageSize(size);

        // Page size is owned by the LayerClient, so no need to notify it of
        // this change.
        mPanZoomController.geometryChanged(false);
        mView.requestRender();
    }

    /**
     * Sets the entire viewport metrics at once. This function does not notify the layer client or
     * the pan/zoom controller, so you will need to call notifyLayerClientOfGeometryChange() or
     * notifyPanZoomControllerOfGeometryChange() after calling this.
     */
    public void setViewportMetrics(ViewportMetrics viewport) {
        mViewportMetrics = new ViewportMetrics(viewport);
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    public void scaleTo(float zoomFactor) {
        scaleWithFocus(zoomFactor, new PointF(0,0));
    }

    public void scaleWithFocus(float zoomFactor, PointF focus) {
        mViewportMetrics.scaleTo(zoomFactor, focus);

        // We assume the zoom level will only be modified by the
        // PanZoomController, so no need to notify it of this change.
        notifyLayerClientOfGeometryChange();
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    public void scaleWithOrigin(float zoomFactor, PointF origin) {
        mViewportMetrics.setOrigin(origin);
        scaleTo(zoomFactor);
    }

    public boolean post(Runnable action) { return mView.post(action); }

    public void setOnTouchListener(OnTouchListener onTouchListener) {
        mOnTouchListener = onTouchListener;
    }

    /**
     * The view as well as the controller itself use this method to notify the layer client that
     * the geometry changed.
     */
    public void notifyLayerClientOfGeometryChange() {
        if (mLayerClient != null)
            mLayerClient.geometryChanged();
    }

    /** Informs the pan/zoom controller that the viewport metrics changed. */
    public void notifyPanZoomControllerOfGeometryChange(boolean abortFling) {
        if (mPanZoomController != null)
            mPanZoomController.geometryChanged(abortFling);
    }

    /**
     * Returns true if this controller is fine with performing a redraw operation or false if it
     * would prefer that the action didn't take place.
     */
    public boolean getRedrawHint() {
        if (mForceRedraw) {
            mForceRedraw = false;
            return true;
        }

        return aboutToCheckerboard() && mPanZoomController.getRedrawHint();
    }

    private RectF getTileRect() {
        float x = mRootLayer.getOrigin().x, y = mRootLayer.getOrigin().y;
        return new RectF(x, y, x + TILE_WIDTH, y + TILE_HEIGHT);
    }

    public RectF restrictToPageSize(RectF aRect) {
        FloatSize pageSize = getPageSize();
        return RectUtils.restrict(aRect, new RectF(0, 0, pageSize.width, pageSize.height));
    }

    // Returns true if a checkerboard is about to be visible.
    private boolean aboutToCheckerboard() {
        // Increase the size of the viewport (and clamp to page boundaries), and
        // intersect it with the tile's displayport to determine whether we're
        // close to checkerboarding.
        FloatSize pageSize = getPageSize();
        RectF adjustedViewport = RectUtils.expand(getViewport(), DANGER_ZONE_X, DANGER_ZONE_Y);
        if (adjustedViewport.top < 0) adjustedViewport.top = 0;
        if (adjustedViewport.left < 0) adjustedViewport.left = 0;
        if (adjustedViewport.right > pageSize.width) adjustedViewport.right = pageSize.width;
        if (adjustedViewport.bottom > pageSize.height) adjustedViewport.bottom = pageSize.height;

        return !getTileRect().contains(adjustedViewport);
    }

    /**
     * Converts a point from layer view coordinates to layer coordinates. In other words, given a
     * point measured in pixels from the top left corner of the layer view, returns the point in
     * pixels measured from the top left corner of the root layer, in the coordinate system of the
     * layer itself. This method is used by the viewport controller as part of the process of
     * translating touch events to Gecko's coordinate system.
     */
    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        if (mRootLayer == null)
            return null;

        // Undo the transforms.
        PointF origin = mViewportMetrics.getOrigin();
        PointF newPoint = new PointF(origin.x, origin.y);
        newPoint.offset(viewPoint.x, viewPoint.y);

        Point rootOrigin = mRootLayer.getOrigin();
        newPoint.offset(-rootOrigin.x, -rootOrigin.y);

        return newPoint;
    }

    /*
     * Gesture detection. This is handled only at a high level in this class; we dispatch to the
     * pan/zoom controller to do the dirty work.
     */
    public boolean onTouchEvent(MotionEvent event) {
        if (mPanZoomController.onTouchEvent(event))
            return true;
        if (mOnTouchListener != null)
            return mOnTouchListener.onTouch(mView, event);
        return false;
    }
}

