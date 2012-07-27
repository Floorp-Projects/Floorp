/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.ui.SimpleScaleGestureDetector;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.RectF;
import android.view.GestureDetector;

/**
 * The layer controller manages a tile that represents the visible page. It does panning and
 * zooming natively by delegating to a panning/zooming controller. Touch events can be dispatched
 * to a higher-level view.
 *
 * Many methods require that the monitor be held, with a synchronized (controller) { ... } block.
 */
public class LayerController {
    private static final String LOGTAG = "GeckoLayerController";

    private Layer mRootLayer;                   /* The root layer. */
    private LayerView mView;                    /* The main rendering view. */
    private Context mContext;                   /* The current context. */

    /* This is volatile so that we can read and write to it from different threads.
     * We avoid synchronization to make getting the viewport metrics from
     * the compositor as cheap as possible. The viewport is immutable so
     * we don't need to worry about anyone mutating it while we're reading from it.
     * Specifically:
     * 1) reading mViewportMetrics from any thread is fine without synchronization
     * 2) writing to mViewportMetrics requires synchronizing on the layer controller object
     * 3) whenver reading multiple fields from mViewportMetrics without synchronization (i.e. in
     *    case 1 above) you should always frist grab a local copy of the reference, and then use
     *    that because mViewportMetrics might get reassigned in between reading the different
     *    fields. */
    private volatile ImmutableViewportMetrics mViewportMetrics;   /* The current viewport metrics. */

    /*
     * The panning and zooming controller, which interprets pan and zoom gestures for us and
     * updates our visible rect appropriately.
     */
    private PanZoomController mPanZoomController;

    private GeckoLayerClient mLayerClient;          /* The layer client. */

    /* The new color for the checkerboard. */
    private int mCheckerboardColor = Color.WHITE;
    private boolean mCheckerboardShouldShowChecks;

    private boolean mAllowZoom;
    private float mDefaultZoom;
    private float mMinZoom;
    private float mMaxZoom;

    private boolean mForceRedraw;

    public LayerController(Context context) {
        mContext = context;

        mForceRedraw = true;
        mViewportMetrics = new ImmutableViewportMetrics(new ViewportMetrics());
        mPanZoomController = new PanZoomController(this);
        mView = new LayerView(context, this);
        mCheckerboardShouldShowChecks = true;
    }

    public void setRoot(Layer layer) { mRootLayer = layer; }

    public void setLayerClient(GeckoLayerClient layerClient) {
        mLayerClient = layerClient;
        layerClient.setLayerController(this);
    }

    public void destroy() {
        mPanZoomController.destroy();
    }

    public void setForceRedraw() {
        mForceRedraw = true;
    }

    public Layer getRoot()                        { return mRootLayer; }
    public LayerView getView()                    { return mView; }
    public Context getContext()                   { return mContext; }
    public ImmutableViewportMetrics getViewportMetrics()   { return mViewportMetrics; }

    public RectF getViewport() {
        return mViewportMetrics.getViewport();
    }

    public RectF getCssViewport() {
        return mViewportMetrics.getCssViewport();
    }

    public FloatSize getViewportSize() {
        return mViewportMetrics.getSize();
    }

    public RectF getPageRect() {
        return mViewportMetrics.getPageRect();
    }

    public RectF getCssPageRect() {
        return mViewportMetrics.getCssPageRect();
    }

    public PointF getOrigin() {
        return mViewportMetrics.getOrigin();
    }

    public float getZoomFactor() {
        return mViewportMetrics.zoomFactor;
    }

    public Bitmap getBackgroundPattern()    { return getDrawable("tabs_tray_selected_bg"); }
    public Bitmap getShadowPattern()        { return getDrawable("shadow"); }

    public PanZoomController getPanZoomController()                                 { return mPanZoomController; }
    public GestureDetector.OnGestureListener getGestureListener()                   { return mPanZoomController; }
    public SimpleScaleGestureDetector.SimpleScaleGestureListener getScaleGestureListener() {
        return mPanZoomController;
    }
    public GestureDetector.OnDoubleTapListener getDoubleTapListener()               { return mPanZoomController; }

    private Bitmap getDrawable(String name) {
        Resources resources = mContext.getResources();
        int resourceID = resources.getIdentifier(name, "drawable", mContext.getPackageName());
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        return BitmapFactory.decodeResource(mContext.getResources(), resourceID, options);
    }

    /**
     * The view calls this function to indicate that the viewport changed size. It must hold the
     * monitor while calling it.
     *
     * TODO: Refactor this to use an interface. Expose that interface only to the view and not
     * to the layer client. That way, the layer client won't be tempted to call this, which might
     * result in an infinite loop.
     */
    public void setViewportSize(FloatSize size) {
        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.setSize(size);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        if (mLayerClient != null) {
            mLayerClient.viewportSizeChanged();
        }
    }

    /** Scrolls the viewport by the given offset. You must hold the monitor while calling this. */
    public void scrollBy(PointF point) {
        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        PointF origin = viewportMetrics.getOrigin();
        origin.offset(point.x, point.y);
        viewportMetrics.setOrigin(origin);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        notifyLayerClientOfGeometryChange();
        mView.requestRender();
    }

    /** Sets the current page rect. You must hold the monitor while calling this. */
    public void setPageRect(RectF rect, RectF cssRect) {
        // Since the "rect" is always just a multiple of "cssRect" we don't need to
        // check both; this function assumes that both "rect" and "cssRect" are relative
        // the zoom factor in mViewportMetrics.
        if (mViewportMetrics.getCssPageRect().equals(cssRect))
            return;

        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.setPageRect(rect, cssRect);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        // Page size is owned by the layer client, so no need to notify it of
        // this change.

        mView.post(new Runnable() {
            public void run() {
                mPanZoomController.pageRectUpdated();
                mView.requestRender();
            }
        });
    }

    /**
     * Sets the entire viewport metrics at once. This function does not notify the layer client or
     * the pan/zoom controller, so you will need to call notifyLayerClientOfGeometryChange() or
     * notifyPanZoomControllerOfGeometryChange() after calling this. You must hold the monitor
     * while calling this.
     */
    public void setViewportMetrics(ViewportMetrics viewport) {
        mViewportMetrics = new ImmutableViewportMetrics(viewport);
        mView.requestRender();
    }

    public void setAnimationTarget(ViewportMetrics viewport) {
        if (mLayerClient != null) {
            // We know what the final viewport of the animation is going to be, so
            // immediately request a draw of that area by setting the display port
            // accordingly. This way we should have the content pre-rendered by the
            // time the animation is done.
            ImmutableViewportMetrics metrics = new ImmutableViewportMetrics(viewport);
            DisplayPortMetrics displayPort = DisplayPortCalculator.calculate(metrics, null);
            mLayerClient.adjustViewport(displayPort);
        }
    }

    /**
     * Scales the viewport, keeping the given focus point in the same place before and after the
     * scale operation. You must hold the monitor while calling this.
     */
    public void scaleWithFocus(float zoomFactor, PointF focus) {
        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.scaleTo(zoomFactor, focus);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        // We assume the zoom level will only be modified by the
        // PanZoomController, so no need to notify it of this change.
        notifyLayerClientOfGeometryChange();
        mView.requestRender();
    }

    public boolean post(Runnable action) { return mView.post(action); }

    /**
     * The view as well as the controller itself use this method to notify the layer client that
     * the geometry changed.
     */
    public void notifyLayerClientOfGeometryChange() {
        if (mLayerClient != null)
            mLayerClient.geometryChanged();
    }

    /** Aborts any pan/zoom animation that is currently in progress. */
    public void abortPanZoomAnimation() {
        if (mPanZoomController != null) {
            mView.post(new Runnable() {
                public void run() {
                    mPanZoomController.abortAnimation();
                }
            });
        }
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

        if (!mPanZoomController.getRedrawHint()) {
            return false;
        }

        return DisplayPortCalculator.aboutToCheckerboard(mViewportMetrics,
                mPanZoomController.getVelocityVector(), mLayerClient.getDisplayPort());
    }

    /**
     * Converts a point from layer view coordinates to layer coordinates. In other words, given a
     * point measured in pixels from the top left corner of the layer view, returns the point in
     * pixels measured from the last scroll position we sent to Gecko, in CSS pixels. Assuming the
     * events being sent to Gecko are processed in FIFO order, this calculation should always be
     * correct.
     */
    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        if (mLayerClient == null) {
            return null;
        }

        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        PointF origin = viewportMetrics.getOrigin();
        float zoom = viewportMetrics.zoomFactor;
        ViewportMetrics geckoViewport = mLayerClient.getGeckoViewportMetrics();
        PointF geckoOrigin = geckoViewport.getOrigin();
        float geckoZoom = geckoViewport.getZoomFactor();

        // viewPoint + origin gives the coordinate in device pixels from the top-left corner of the page.
        // Divided by zoom, this gives us the coordinate in CSS pixels from the top-left corner of the page.
        // geckoOrigin / geckoZoom is where Gecko thinks it is (scrollTo position) in CSS pixels from
        // the top-left corner of the page. Subtracting the two gives us the offset of the viewPoint from
        // the current Gecko coordinate in CSS pixels.
        PointF layerPoint = new PointF(
                ((viewPoint.x + origin.x) / zoom) - (geckoOrigin.x / geckoZoom),
                ((viewPoint.y + origin.y) / zoom) - (geckoOrigin.y / geckoZoom));

        return layerPoint;
    }

    /** Retrieves whether we should show checkerboard checks or not. */
    public boolean checkerboardShouldShowChecks() {
        return mCheckerboardShouldShowChecks;
    }

    /** Retrieves the color that the checkerboard should be. */
    public int getCheckerboardColor() {
        return mCheckerboardColor;
    }

    /** Sets whether or not the checkerboard should show checkmarks. */
    public void setCheckerboardShowChecks(boolean showChecks) {
        mCheckerboardShouldShowChecks = showChecks;
        mView.requestRender();
    }

    /** Sets a new color for the checkerboard. */
    public void setCheckerboardColor(int newColor) {
        mCheckerboardColor = newColor;
        mView.requestRender();
    }

    public void setAllowZoom(final boolean aValue) {
        mAllowZoom = aValue;
    }

    public boolean getAllowZoom() {
        return mAllowZoom;
    }

    public void setDefaultZoom(float aValue) {
        mDefaultZoom = aValue;
    }

    public float getDefaultZoom() {
        return mDefaultZoom;
    }

    public void setMinZoom(float aValue) {
        mMinZoom = aValue;
    }

    public float getMinZoom() {
        return mMinZoom;
    }

    public void setMaxZoom(float aValue) {
        mMaxZoom = aValue;
    }

    public float getMaxZoom() {
        return mMaxZoom;
    }
}
