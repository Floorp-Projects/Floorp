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
    private RectF mDisplayPort;

    private VirtualLayer mRootLayer;

    /* The viewport that Gecko is currently displaying. */
    private ViewportMetrics mGeckoViewport;

    private String mLastCheckerboardColor;

    /* Used by robocop for testing purposes */
    private DrawListener mDrawListener;

    /* Used as a temporary ViewTransform by getViewTransform */
    private ViewTransform mCurrentViewTransform;

    public GeckoLayerClient(Context context) {
        // we can fill these in with dummy values because they are always written
        // to before being read
        mScreenSize = new IntSize(0, 0);
        mBufferSize = new IntSize(0, 0);
        mDisplayPort = new RectF();
        mCurrentViewTransform = new ViewTransform(0, 0, 1);
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    void setLayerController(LayerController layerController) {
        mLayerController = layerController;

        layerController.setRoot(mRootLayer);
        if (mGeckoViewport != null) {
            layerController.setViewportMetrics(mGeckoViewport);
        }

        GeckoAppShell.registerGeckoEventListener("Viewport:Update", this);

        sendResizeEventIfNecessary(false);

        LayerView view = layerController.getView();
        view.setListener(this);

        mLayerRenderer = new LayerRenderer(view);
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public boolean beginDrawing(int width, int height, String metadata) {
        // If we've changed surface types, cancel this draw
        if (initializeVirtualLayer()) {
            Log.e(LOGTAG, "### Cancelling draw due to virtual layer initialization");
            return false;
        }

        try {
            JSONObject viewportObject = new JSONObject(metadata);
            mGeckoViewport = new ViewportMetrics(viewportObject);
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
            RectF position = mGeckoViewport.getViewport();
            mRootLayer.setPositionAndResolution(RectUtils.round(position), mGeckoViewport.getZoomFactor());
        }

        Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - endDrawing");
        /* Used by robocop for testing purposes */
        if (mDrawListener != null) {
            mDrawListener.drawFinished();
        }
    }

    RectF getDisplayPort() {
        return mDisplayPort;
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
        return size;
    }

    public Bitmap getBitmap() {
        return null;
    }

    void viewportSizeChanged() {
        // here we send gecko a resize message. The code in browser.js is responsible for
        // picking up on that resize event, modifying the viewport as necessary, and informing
        // us of the new viewport.
        sendResizeEventIfNecessary(true);
        // the following call also sends gecko a message, which will be processed after the resize
        // message above has updated the viewport. this message ensures that if we have just put
        // focus in a text field, we scroll the content so that the text field is in view.
        GeckoAppShell.viewSizeChanged();
    }

    private void updateDisplayPort() {
        float desiredXMargins = 2 * DEFAULT_DISPLAY_PORT_MARGIN;
        float desiredYMargins = 2 * DEFAULT_DISPLAY_PORT_MARGIN;

        ImmutableViewportMetrics metrics = mLayerController.getViewportMetrics(); 

        // we need to avoid having a display port that is larger than the page, or we will end up
        // painting things outside the page bounds (bug 729169). we simultaneously need to make
        // the display port as large as possible so that we redraw less.

        // figure out how much of the desired buffer amount we can actually use on the horizontal axis
        float xBufferAmount = Math.min(desiredXMargins, metrics.pageSizeWidth - metrics.getWidth());
        // if we reduced the buffer amount on the horizontal axis, we should take that saved memory and
        // use it on the vertical axis
        float savedPixels = (desiredXMargins - xBufferAmount) * (metrics.getHeight() + desiredYMargins);
        float extraYAmount = (float)Math.floor(savedPixels / (metrics.getWidth() + xBufferAmount));
        float yBufferAmount = Math.min(desiredYMargins + extraYAmount, metrics.pageSizeHeight - metrics.getHeight());
        // and the reverse - if we shrunk the buffer on the vertical axis we can add it to the horizontal
        if (xBufferAmount == desiredXMargins && yBufferAmount < desiredYMargins) {
            savedPixels = (desiredYMargins - yBufferAmount) * (metrics.getWidth() + xBufferAmount);
            float extraXAmount = (float)Math.floor(savedPixels / (metrics.getHeight() + yBufferAmount));
            xBufferAmount = Math.min(xBufferAmount + extraXAmount, metrics.pageSizeWidth - metrics.getWidth());
        }

        // and now calculate the display port margins based on how much buffer we've decided to use and
        // the page bounds, ensuring we use all of the available buffer amounts on one side or the other
        // on any given axis. (i.e. if we're scrolled to the top of the page, the vertical buffer is
        // entirely below the visible viewport, but if we're halfway down the page, the vertical buffer
        // is split).
        float leftMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.viewportRectLeft);
        float rightMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.pageSizeWidth - (metrics.viewportRectLeft + metrics.getWidth()));
        if (leftMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            rightMargin = xBufferAmount - leftMargin;
        } else if (rightMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            leftMargin = xBufferAmount - rightMargin;
        } else if (!FloatUtils.fuzzyEquals(leftMargin + rightMargin, xBufferAmount)) {
            float delta = xBufferAmount - leftMargin - rightMargin;
            leftMargin += delta / 2;
            rightMargin += delta / 2;
        }

        float topMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.viewportRectTop);
        float bottomMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.pageSizeHeight - (metrics.viewportRectTop + metrics.getHeight()));
        if (topMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            bottomMargin = yBufferAmount - topMargin;
        } else if (bottomMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            topMargin = yBufferAmount - bottomMargin;
        } else if (!FloatUtils.fuzzyEquals(topMargin + bottomMargin, yBufferAmount)) {
            float delta = yBufferAmount - topMargin - bottomMargin;
            topMargin += delta / 2;
            bottomMargin += delta / 2;
        }

        // note that unless the viewport size changes, or the page dimensions change (either because of
        // content changes or zooming), the size of the display port should remain constant. this
        // is intentional to avoid re-creating textures and all sorts of other reallocations in the
        // draw and composition code.

        mDisplayPort.left = metrics.viewportRectLeft - leftMargin;
        mDisplayPort.top = metrics.viewportRectTop - topMargin;
        mDisplayPort.right = metrics.viewportRectRight + rightMargin;
        mDisplayPort.bottom = metrics.viewportRectBottom + bottomMargin;
    }

    private void adjustViewport() {
        ViewportMetrics viewportMetrics =
            new ViewportMetrics(mLayerController.getViewportMetrics());

        viewportMetrics.setViewport(viewportMetrics.getClampedViewport());

        updateDisplayPort();
        GeckoAppShell.sendEventToGecko(GeckoEvent.createViewportEvent(viewportMetrics, mDisplayPort));
    }

    /** Implementation of GeckoEventResponder/GeckoEventListener. */
    public void handleMessage(String event, JSONObject message) {
        if ("Viewport:Update".equals(event)) {
            try {
                ViewportMetrics newMetrics = new ViewportMetrics(message);
                synchronized (mLayerController) {
                    // keep the old viewport size, but update everything else
                    ImmutableViewportMetrics oldMetrics = mLayerController.getViewportMetrics();
                    newMetrics.setSize(oldMetrics.getSize());
                    mLayerController.setViewportMetrics(newMetrics);
                    mLayerController.abortPanZoomAnimation(false);
                }
            } catch (JSONException e) {
                Log.e(LOGTAG, "Unable to create viewport metrics in " + event + " handler", e);
            }
        }
    }

    /** Implementation of GeckoEventResponder. */
    public String getResponse() {
        // We are responding to the events handled in handleMessage() above with the
        // display port we want. Note that all messages we are currently handling
        // (just Viewport:Update) require this response, so we can just return this
        // indiscriminately.
        updateDisplayPort();
        return RectUtils.toJSON(mDisplayPort);
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
    public void setFirstPaintViewport(float offsetX, float offsetY, float zoom, float pageWidth, float pageHeight) {
        synchronized (mLayerController) {
            ViewportMetrics currentMetrics = new ViewportMetrics(mLayerController.getViewportMetrics());
            currentMetrics.setOrigin(new PointF(offsetX, offsetY));
            currentMetrics.setZoomFactor(zoom);
            currentMetrics.setPageSize(new FloatSize(pageWidth, pageHeight));
            mLayerController.setViewportMetrics(currentMetrics);
            // At this point, we have just switched to displaying a different document than we
            // we previously displaying. This means we need to abort any panning/zooming animations
            // that are in progress and send an updated display port request to browser.js as soon
            // as possible. We accomplish this by passing true to abortPanZoomAnimation, which
            // sends the request after aborting the animation. The display port request is actually
            // a full viewport update, which is fine because if browser.js has somehow moved to
            // be out of sync with this first-paint viewport, then we force them back in sync.
            mLayerController.abortPanZoomAnimation(true);
        }
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void setPageSize(float zoom, float pageWidth, float pageHeight) {
        synchronized (mLayerController) {
            // adjust the page dimensions to account for differences in zoom
            // between the rendered content (which is what the compositor tells us)
            // and our zoom level (which may have diverged).
            float ourZoom = mLayerController.getZoomFactor();
            pageWidth = pageWidth * ourZoom / zoom;
            pageHeight = pageHeight * ourZoom /zoom;
            mLayerController.setPageSize(new FloatSize(pageWidth, pageHeight));
            // Here the page size of the document has changed, but the document being displayed
            // is still the same. Therefore, we don't need to send anything to browser.js; any
            // changes we need to make to the display port will get sent the next time we call
            // adjustViewport().
        }
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    /* This functions needs to be fast because it is called by the compositor every frame.
     * It avoids taking any locks or allocating any objects. We keep around a
     * mCurrentViewTransform so we don't need to allocate a new ViewTransform
     * everytime we're called. NOTE: we could probably switch to returning a ImmutableViewportMetrics
     * which would avoid the copy into mCurrentViewTransform. */
    public ViewTransform getViewTransform() {
        // getViewportMetrics is thread safe so we don't need to synchronize
        // on myLayerController.
        ImmutableViewportMetrics viewportMetrics = mLayerController.getViewportMetrics();
        mCurrentViewTransform.x = viewportMetrics.viewportRectLeft;
        mCurrentViewTransform.y = viewportMetrics.viewportRectTop;
        mCurrentViewTransform.scale = viewportMetrics.zoomFactor;
        return mCurrentViewTransform;
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

