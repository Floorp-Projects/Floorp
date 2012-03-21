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
import org.mozilla.gecko.ui.PanZoomController;
import org.mozilla.gecko.ui.SimpleScaleGestureDetector;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Tab;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.view.MotionEvent;
import android.view.GestureDetector;
import android.view.ScaleGestureDetector;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import java.lang.Math;
import java.util.Timer;
import java.util.TimerTask;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The layer controller manages a tile that represents the visible page. It does panning and
 * zooming natively by delegating to a panning/zooming controller. Touch events can be dispatched
 * to a higher-level view.
 *
 * Many methods require that the monitor be held, with a synchronized (controller) { ... } block.
 */
public class LayerController implements Tabs.OnTabsChangedListener {
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

    private boolean mWaitForTouchListeners;

    private PanZoomController mPanZoomController;
    /*
     * The panning and zooming controller, which interprets pan and zoom gestures for us and
     * updates our visible rect appropriately.
     */

    private OnTouchListener mOnTouchListener;       /* The touch listener. */
    private GeckoLayerClient mLayerClient;          /* The layer client. */

    /* The new color for the checkerboard. */
    private int mCheckerboardColor;
    private boolean mCheckerboardShouldShowChecks;

    private boolean mForceRedraw;

    /* The extra area on the sides of the page that we want to buffer to help with
     * smooth, asynchronous scrolling. Depending on a device's support for NPOT
     * textures, this may be rounded up to the nearest power of two.
     */
    public static final IntSize MIN_BUFFER = new IntSize(512, 1024);

    /* If the visible rect is within the danger zone (measured in pixels from each edge of a tile),
     * we start aggressively redrawing to minimize checkerboarding. */
    private static final int DANGER_ZONE_X = 75;
    private static final int DANGER_ZONE_Y = 150;

    /* The time limit for pages to respond with preventDefault on touchevents
     * before we begin panning the page */
    private int mTimeout = 200;

    private boolean allowDefaultActions = true;
    private Timer allowDefaultTimer =  null;
    private PointF initialTouchLocation = null;

    private static Pattern sColorPattern;

    public LayerController(Context context) {
        mContext = context;

        mForceRedraw = true;
        mViewportMetrics = new ImmutableViewportMetrics(new ViewportMetrics());
        mPanZoomController = new PanZoomController(this);
        mView = new LayerView(context, this);
        mCheckerboardShouldShowChecks = true;

        Tabs.registerOnTabsChangedListener(this);

        mTimeout = ViewConfiguration.getLongPressTimeout();
    }

    public void onDestroy() {
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void setRoot(Layer layer) { mRootLayer = layer; }

    public void setLayerClient(GeckoLayerClient layerClient) {
        mLayerClient = layerClient;
        layerClient.setLayerController(this);
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
        return mViewportMetrics.zoomFactor;
    }

    public Bitmap getBackgroundPattern()    { return getDrawable("background"); }
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
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
    }

    /** Sets the current page size. You must hold the monitor while calling this. */
    public void setPageSize(FloatSize size) {
        if (mViewportMetrics.getPageSize().fuzzyEquals(size))
            return;

        ViewportMetrics viewportMetrics = new ViewportMetrics(mViewportMetrics);
        viewportMetrics.setPageSize(size);
        mViewportMetrics = new ImmutableViewportMetrics(viewportMetrics);

        // Page size is owned by the layer client, so no need to notify it of
        // this change.

        mView.post(new Runnable() {
            public void run() {
                mPanZoomController.pageSizeUpdated();
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
        // this function may or may not be called on the UI thread,
        // but repositionPluginViews must only be called on the UI thread.
        GeckoApp.mAppContext.runOnUiThread(new Runnable() {
            public void run() {
                GeckoApp.mAppContext.repositionPluginViews(false);
            }
        });
        mView.requestRender();
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
        GeckoApp.mAppContext.repositionPluginViews(false);
        mView.requestRender();
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

        return aboutToCheckerboard();
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

        RectF displayPort = (mLayerClient == null ? new RectF() : mLayerClient.getDisplayPort());
        return !displayPort.contains(adjustedViewport);
    }

    /**
     * Converts a point from layer view coordinates to layer coordinates. In other words, given a
     * point measured in pixels from the top left corner of the layer view, returns the point in
     * pixels measured from the last scroll position we sent to Gecko, in CSS pixels. Assuming the
     * events being sent to Gecko are processed in FIFO order, this calculation should always be
     * correct.
     */
    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
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

    /*
     * Gesture detection. This is handled only at a high level in this class; we dispatch to the
     * pan/zoom controller to do the dirty work.
     */
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getAction();
        PointF point = new PointF(event.getX(), event.getY());

        // this will only match the first touchstart in a series
        if ((action & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_DOWN) {
            initialTouchLocation = point;
            allowDefaultActions = !mWaitForTouchListeners;

            // if we have a timer, this may be a double tap,
            // cancel the current timer but don't clear the event queue
            if (allowDefaultTimer != null) {
              allowDefaultTimer.cancel();
            } else {
              // if we don't have a timer, make sure we remove any old events
              mView.clearEventQueue();
            }
            allowDefaultTimer = new Timer();
            allowDefaultTimer.schedule(new TimerTask() {
                public void run() {
                    post(new Runnable() {
                        public void run() {
                            preventPanning(false);
                        }
                    });
                }
            }, mTimeout);
        }

        // After the initial touch, ignore touch moves until they exceed a minimum distance.
        if (initialTouchLocation != null && (action & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_MOVE) {
            if (PointUtils.subtract(point, initialTouchLocation).length() > PanZoomController.PAN_THRESHOLD) {
                initialTouchLocation = null;
            } else {
                return !allowDefaultActions;
            }
        }

        // send the event to content
        if (mOnTouchListener != null)
            mOnTouchListener.onTouch(mView, event);

        return !allowDefaultActions;
    }

    public void preventPanning(boolean aValue) {
        if (allowDefaultTimer != null) {
            allowDefaultTimer.cancel();
            allowDefaultTimer = null;
        }
        if (aValue == allowDefaultActions) {
            allowDefaultActions = !aValue;
    
            if (aValue) {
                mView.clearEventQueue();
                mPanZoomController.cancelTouch();
            } else {
                mView.processEventQueue();
            }
        }
    }

    public void onTabChanged(Tab tab, Tabs.TabEvents msg) {
        if ((Tabs.getInstance().isSelectedTab(tab) && msg == Tabs.TabEvents.STOP) || msg == Tabs.TabEvents.SELECTED) {
            mWaitForTouchListeners = tab.getHasTouchListeners();
        }
    }
    public void setWaitForTouchListeners(boolean aValue) {
        mWaitForTouchListeners = aValue;
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

    /** Parses and sets a new color for the checkerboard. */
    public void setCheckerboardColor(String newColor) {
        setCheckerboardColor(parseColorFromGecko(newColor));
    }

    // Parses a color from an RGB triple of the form "rgb([0-9]+, [0-9]+, [0-9]+)". If the color
    // cannot be parsed, returns white.
    private static int parseColorFromGecko(String string) {
        if (sColorPattern == null) {
            sColorPattern = Pattern.compile("rgb\\((\\d+),\\s*(\\d+),\\s*(\\d+)\\)");
        }

        Matcher matcher = sColorPattern.matcher(string);
        if (!matcher.matches()) {
            return Color.WHITE;
        }

        int r = Integer.parseInt(matcher.group(1));
        int g = Integer.parseInt(matcher.group(2));
        int b = Integer.parseInt(matcher.group(3));
        return Color.rgb(r, g, b);
    } 

}

