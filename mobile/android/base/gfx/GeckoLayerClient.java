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

import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoEventResponder;
import org.json.JSONException;
import org.json.JSONObject;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;

public class GeckoLayerClient implements GeckoEventResponder,
                                         FlexibleGLSurfaceView.Listener {
    private static final String LOGTAG = "GeckoLayerClient";

    private static final int DEFAULT_DISPLAY_PORT_MARGIN = 300;

    private LayerController mLayerController;
    private LayerRenderer mLayerRenderer;
    private boolean mLayerRendererInitialized;

    private IntSize mScreenSize;
    private IntSize mWindowSize;
    private IntSize mBufferSize;
    private Rect mDisplayPortMargins;

    private VirtualLayer mRootLayer;

    /* The viewport that Gecko is currently displaying. */
    private ViewportMetrics mGeckoViewport;

    /* The viewport that Gecko will display when drawing is finished */
    private ViewportMetrics mNewGeckoViewport;

    private boolean mViewportSizeChanged;
    private boolean mIgnorePaintsPendingViewportSizeChange;
    private boolean mFirstPaint = true;

    // mUpdateViewportOnEndDraw is used to indicate that we received a
    // viewport update notification while drawing. therefore, when the
    // draw finishes, we need to update the entire viewport rather than
    // just the page size. this boolean should always be accessed from
    // inside a transaction, so no synchronization is needed.
    private boolean mUpdateViewportOnEndDraw;

    private String mLastCheckerboardColor;

    /* Used by robocop for testing purposes */
    private DrawListener mDrawListener;

    public GeckoLayerClient(Context context) {
        mScreenSize = new IntSize(0, 0);
        mBufferSize = new IntSize(0, 0);
        mDisplayPortMargins = new Rect(DEFAULT_DISPLAY_PORT_MARGIN,
                                       DEFAULT_DISPLAY_PORT_MARGIN,
                                       DEFAULT_DISPLAY_PORT_MARGIN,
                                       DEFAULT_DISPLAY_PORT_MARGIN);
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    void setLayerController(LayerController layerController) {
        mLayerController = layerController;

        layerController.setRoot(mRootLayer);
        if (mGeckoViewport != null) {
            layerController.setViewportMetrics(mGeckoViewport);
        }

        GeckoAppShell.registerGeckoEventListener("Viewport:UpdateAndDraw", this);
        GeckoAppShell.registerGeckoEventListener("Viewport:UpdateLater", this);

        sendResizeEventIfNecessary(false);

        LayerView view = layerController.getView();
        view.setListener(this);

        mLayerRenderer = new LayerRenderer(view);
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public boolean beginDrawing(int width, int height, String metadata) {
        // If the viewport has changed but we still don't have the latest viewport
        // from Gecko, ignore the viewport passed to us from Gecko since that is going
        // to be wrong.
        if (!mFirstPaint && mIgnorePaintsPendingViewportSizeChange) {
            return false;
        }
        mFirstPaint = false;

        // If we've changed surface types, cancel this draw
        if (initializeVirtualLayer()) {
            Log.e(LOGTAG, "### Cancelling draw due to virtual layer initialization");
            return false;
        }

        try {
            JSONObject viewportObject = new JSONObject(metadata);
            mNewGeckoViewport = new ViewportMetrics(viewportObject);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Aborting draw, bad viewport description: " + metadata);
            return false;
        }

        if (mBufferSize.width != width || mBufferSize.height != height) {
            mBufferSize = new IntSize(width, height);
        }

        return true;
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void endDrawing() {
        synchronized (mLayerController) {
            // save and restore the viewport size stored in java; never let the
            // JS-side viewport dimensions override the java-side ones because
            // java is the One True Source of this information, and allowing JS
            // to override can lead to race conditions where this data gets clobbered.
            FloatSize viewportSize = mLayerController.getViewportSize();
            mGeckoViewport = mNewGeckoViewport;
            mGeckoViewport.setSize(viewportSize);

            RectF position = mGeckoViewport.getViewport();
            mRootLayer.setPositionAndResolution(RectUtils.round(position), mGeckoViewport.getZoomFactor());

            if (mUpdateViewportOnEndDraw) {
                mLayerController.setViewportMetrics(mGeckoViewport);
                mLayerController.abortPanZoomAnimation();
                mUpdateViewportOnEndDraw = false;
            } else {
                // Don't adjust page size when zooming unless zoom levels are
                // approximately equal.
                if (FloatUtils.fuzzyEquals(mLayerController.getZoomFactor(),
                        mGeckoViewport.getZoomFactor()))
                    mLayerController.setPageSize(mGeckoViewport.getPageSize());
            }
        }

        Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - endDrawing");
        /* Used by robocop for testing purposes */
        if (mDrawListener != null) {
            mDrawListener.drawFinished();
        }
    }

    RectF getDisplayPort() {
        RectF displayPort = new RectF(mRootLayer.getPosition());
        displayPort.left -= mDisplayPortMargins.left;
        displayPort.top -= mDisplayPortMargins.top;
        displayPort.right += mDisplayPortMargins.right;
        displayPort.bottom += mDisplayPortMargins.bottom;
        return displayPort;
    }

    /* Informs Gecko that the screen size has changed. */
    private void sendResizeEventIfNecessary(boolean force) {
        DisplayMetrics metrics = new DisplayMetrics();
        GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

        IntSize newScreenSize = new IntSize(metrics.widthPixels, metrics.heightPixels);
        IntSize newWindowSize = getBufferSize();

        boolean screenSizeChanged = mScreenSize == null || !mScreenSize.equals(newScreenSize);
        boolean windowSizeChanged = mWindowSize == null || !mWindowSize.equals(newWindowSize);

        if (!force && !screenSizeChanged && !windowSizeChanged) {
            return;
        }

        mScreenSize = newScreenSize;
        mWindowSize = newWindowSize;

        if (screenSizeChanged) {
            Log.i(LOGTAG, "### Screen-size changed to " + mScreenSize);
        }

        if (windowSizeChanged) {
            Log.i(LOGTAG, "### Window-size changed to " + mWindowSize);
        }

        GeckoEvent event = GeckoEvent.createSizeChangedEvent(mWindowSize.width, mWindowSize.height,  // Window (buffer) size
                                                             mScreenSize.width, mScreenSize.height); // Screen size
        GeckoAppShell.sendEventToGecko(event);
    }

    private boolean initializeVirtualLayer() {
        if (mRootLayer != null) {
            return false;
        }

        VirtualLayer virtualLayer = new VirtualLayer(getBufferSize());
        mLayerController.setRoot(virtualLayer);
        mRootLayer = virtualLayer;

        sendResizeEventIfNecessary(true);
        return true;
    }

    private IntSize getBufferSize() {
        View view = mLayerController.getView();
        IntSize size = new IntSize(view.getWidth(), view.getHeight());
        Log.e(LOGTAG, "### getBufferSize " + size);
        return size;
    }

    public Bitmap getBitmap() {
        return null;
    }

    void viewportSizeChanged() {
        mViewportSizeChanged = true;
        mIgnorePaintsPendingViewportSizeChange = true;
    }

    private void adjustViewport() {
        ViewportMetrics viewportMetrics =
            new ViewportMetrics(mLayerController.getViewportMetrics());

        viewportMetrics.setViewport(viewportMetrics.getClampedViewport());

        GeckoAppShell.sendEventToGecko(GeckoEvent.createViewportEvent(viewportMetrics, mDisplayPortMargins));
        if (mViewportSizeChanged) {
            mViewportSizeChanged = false;
            GeckoAppShell.viewSizeChanged();
        }
    }

    /** Implementation of GeckoEventResponder/GeckoEventListener. */
    public void handleMessage(String event, JSONObject message) {
        if ("Viewport:UpdateAndDraw".equals(event)) {
            mUpdateViewportOnEndDraw = true;
            mIgnorePaintsPendingViewportSizeChange = false;

            // Redraw everything.
            Rect rect = new Rect(0, 0, mBufferSize.width, mBufferSize.height);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createDrawEvent(rect));
        } else if ("Viewport:UpdateLater".equals(event)) {
            mUpdateViewportOnEndDraw = true;
            mIgnorePaintsPendingViewportSizeChange = false;
        }
    }

    /** Implementation of GeckoEventResponder. */
    public String getResponse() {
        // We are responding to the events handled in handleMessage() above with
        // the display port margins we want. Note that both messages we are currently
        // handling (Viewport:UpdateAndDraw and Viewport:UpdateLater) require this
        // response, so we can just return this indiscriminately.
        return RectUtils.toJSON(mDisplayPortMargins);
    }

    void geometryChanged() {
        /* Let Gecko know if the screensize has changed */
        sendResizeEventIfNecessary(false);
        if (mLayerController.getRedrawHint())
            adjustViewport();
    }

    public int getWidth() {
        return mBufferSize.width;
    }

    public int getHeight() {
        return mBufferSize.height;
    }

    public ViewportMetrics getGeckoViewportMetrics() {
        // Return a copy, as we modify this inside the Gecko thread
        if (mGeckoViewport != null)
            return new ViewportMetrics(mGeckoViewport);
        return null;
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public ViewTransform getViewTransform() {
        // NB: We don't begin a transaction here because this can be called in a synchronous
        // manner between beginDrawing() and endDrawing(), and that will cause a deadlock.

        synchronized (mLayerController) {
            ViewportMetrics viewportMetrics = mLayerController.getViewportMetrics();
            PointF viewportOrigin = viewportMetrics.getOrigin();
            float scrollX = viewportOrigin.x; 
            float scrollY = viewportOrigin.y;
            float zoomFactor = viewportMetrics.getZoomFactor();
            return new ViewTransform(scrollX, scrollY, zoomFactor);
        }
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public LayerRenderer.Frame createFrame() {
        // Create the shaders and textures if necessary.
        if (!mLayerRendererInitialized) {
            mLayerRenderer.checkMonitoringEnabled();
            mLayerRenderer.createDefaultProgram();
            mLayerRendererInitialized = true;
        }

        // Build the contexts and create the frame.
        Layer.RenderContext pageContext = mLayerRenderer.createPageContext();
        Layer.RenderContext screenContext = mLayerRenderer.createScreenContext();
        return mLayerRenderer.createFrame(pageContext, screenContext);
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void activateProgram() {
        mLayerRenderer.activateDefaultProgram();
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void deactivateProgram() {
        mLayerRenderer.deactivateDefaultProgram();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void renderRequested() {
        GeckoAppShell.scheduleComposite();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void compositionPauseRequested() {
        GeckoAppShell.schedulePauseComposition();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void compositionResumeRequested() {
        GeckoAppShell.scheduleResumeComposition();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void surfaceChanged(int width, int height) {
        compositionPauseRequested();
        mLayerController.setViewportSize(new FloatSize(width, height));
        compositionResumeRequested();
        renderRequested();
    }

    /** Used by robocop for testing purposes. Not for production use! This is called via reflection by robocop. */
    public void setDrawListener(DrawListener listener) {
        mDrawListener = listener;
    }

    /** Used by robocop for testing purposes. Not for production use! This is used via reflection by robocop. */
    public interface DrawListener {
        public void drawFinished();
    }
}

