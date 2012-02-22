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
import org.mozilla.gecko.GeckoEventListener;
import org.json.JSONException;
import org.json.JSONObject;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class GeckoLayerClient implements GeckoEventListener,
                                         FlexibleGLSurfaceView.Listener,
                                         VirtualLayer.Listener {
    private static final String LOGTAG = "GeckoLayerClient";

    private LayerController mLayerController;
    private LayerRenderer mLayerRenderer;
    private boolean mLayerRendererInitialized;

    private IntSize mScreenSize;
    private IntSize mWindowSize;
    private IntSize mBufferSize;

    private Layer mTileLayer;

    /* The viewport that Gecko is currently displaying. */
    private ViewportMetrics mGeckoViewport;

    /* The viewport that Gecko will display when drawing is finished */
    private ViewportMetrics mNewGeckoViewport;

    private static final long MIN_VIEWPORT_CHANGE_DELAY = 25L;
    private long mLastViewportChangeTime;
    private boolean mPendingViewportAdjust;
    private boolean mViewportSizeChanged;

    // mUpdateViewportOnEndDraw is used to indicate that we received a
    // viewport update notification while drawing. therefore, when the
    // draw finishes, we need to update the entire viewport rather than
    // just the page size. this boolean should always be accessed from
    // inside a transaction, so no synchronization is needed.
    private boolean mUpdateViewportOnEndDraw;

    private String mLastCheckerboardColor;

    private static Pattern sColorPattern;

    /* Used by robocop for testing purposes */
    private DrawListener mDrawListener;

    public GeckoLayerClient(Context context) {
        mScreenSize = new IntSize(0, 0);
        mBufferSize = new IntSize(0, 0);
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    void setLayerController(LayerController layerController) {
        mLayerController = layerController;

        layerController.setRoot(mTileLayer);
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
    public Rect beginDrawing(int width, int height, int tileWidth, int tileHeight,
                             String metadata, boolean hasDirectTexture) {
        Log.e(LOGTAG, "### beginDrawing " + width + " " + height + " " + tileWidth + " " +
              tileHeight + " " + hasDirectTexture);

        // If we've changed surface types, cancel this draw
        if (handleDirectTextureChange(hasDirectTexture)) {
            Log.e(LOGTAG, "### Cancelling draw due to direct texture change");
            return null;
        }

        try {
            JSONObject viewportObject = new JSONObject(metadata);
            mNewGeckoViewport = new ViewportMetrics(viewportObject);

            Log.e(LOGTAG, "### beginDrawing new Gecko viewport " + mNewGeckoViewport);

            // Update the background color, if it's present.
            String backgroundColorString = viewportObject.optString("backgroundColor");
            if (backgroundColorString != null && !backgroundColorString.equals(mLastCheckerboardColor)) {
                mLastCheckerboardColor = backgroundColorString;
                mLayerController.setCheckerboardColor(parseColorFromGecko(backgroundColorString));
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Aborting draw, bad viewport description: " + metadata);
            return null;
        }


        // Make sure we don't spend time painting areas we aren't interested in.
        // Only do this if the Gecko viewport isn't going to override our viewport.
        Rect bufferRect = new Rect(0, 0, width, height);

        if (!mUpdateViewportOnEndDraw) {
            // First, find out our ideal displayport. This would be what we would
            // send to Gecko if adjustViewport were called now.
            ViewportMetrics currentMetrics = mLayerController.getViewportMetrics();
            PointF currentBestOrigin = RectUtils.getOrigin(currentMetrics.getClampedViewport());

            Rect currentRect = RectUtils.round(new RectF(currentBestOrigin.x, currentBestOrigin.y,
                                                         currentBestOrigin.x + width, currentBestOrigin.y + height));

            // Second, store Gecko's displayport.
            PointF currentOrigin = mNewGeckoViewport.getDisplayportOrigin();
            bufferRect = RectUtils.round(new RectF(currentOrigin.x, currentOrigin.y,
                                                   currentOrigin.x + width, currentOrigin.y + height));

            int area = width * height;

            // Take the intersection of the two as the area we're interested in rendering.
            if (!bufferRect.intersect(currentRect)) {
                Log.w(LOGTAG, "Prediction would avoid useless paint of " + area + " pixels (100.0%)");
                // If there's no intersection, we have no need to render anything,
                // but make sure to update the viewport size.
                mTileLayer.beginTransaction();
                try {
                    updateViewport(true);
                } finally {
                    mTileLayer.endTransaction();
                }
                return null;
            }

            int wasted = area - (bufferRect.width() * bufferRect.height());
            Log.w(LOGTAG, "Prediction would avoid useless paint of " + wasted + " pixels (" + ((float)wasted * 100.0f / area) + "%)");

            bufferRect.offset(Math.round(-currentOrigin.x), Math.round(-currentOrigin.y));
        }

        mTileLayer.beginTransaction();

        if (mBufferSize.width != width || mBufferSize.height != height) {
            mBufferSize = new IntSize(width, height);
        }

        return bufferRect;
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature.
     *
     * TODO: Would be cleaner if this took an android.graphics.Rect instead, but that would require
     * a little more JNI magic.
     */
    public void endDrawing(int x, int y, int width, int height) {
        synchronized (mLayerController) {
            try {
                updateViewport(!mUpdateViewportOnEndDraw);
                mUpdateViewportOnEndDraw = false;
            } finally {
                mTileLayer.endTransaction();
            }
        }
        Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - endDrawing");

        /* Used by robocop for testing purposes */
        if (mDrawListener != null) {
            mDrawListener.drawFinished(x, y, width, height);
        }
    }

    private void updateViewport(boolean onlyUpdatePageSize) {
        // save and restore the viewport size stored in java; never let the
        // JS-side viewport dimensions override the java-side ones because
        // java is the One True Source of this information, and allowing JS
        // to override can lead to race conditions where this data gets clobbered.
        FloatSize viewportSize = mLayerController.getViewportSize();
        mGeckoViewport = mNewGeckoViewport;
        mGeckoViewport.setSize(viewportSize);

        PointF displayportOrigin = mGeckoViewport.getDisplayportOrigin();
        mTileLayer.setOrigin(PointUtils.round(displayportOrigin));
        mTileLayer.setResolution(mGeckoViewport.getZoomFactor());

        // Set the new origin and resolution instantly.
        mTileLayer.performUpdates(null);

        Log.e(LOGTAG, "### updateViewport onlyUpdatePageSize=" + onlyUpdatePageSize +
              " getTileViewport " + mGeckoViewport);

        if (onlyUpdatePageSize) {
            // Don't adjust page size when zooming unless zoom levels are
            // approximately equal.
            if (FloatUtils.fuzzyEquals(mLayerController.getZoomFactor(),
                    mGeckoViewport.getZoomFactor()))
                mLayerController.setPageSize(mGeckoViewport.getPageSize());
        } else {
            mLayerController.setViewportMetrics(mGeckoViewport);
            mLayerController.abortPanZoomAnimation();
        }
    }

    /* Informs Gecko that the screen size has changed. */
    private void sendResizeEventIfNecessary(boolean force) {
        Log.d(LOGTAG, "### sendResizeEventIfNecessary " + force);

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

        IntSize bufferSize = getBufferSize();
        GeckoEvent event = GeckoEvent.createSizeChangedEvent(mWindowSize.width, mWindowSize.height, // Window (buffer) size
                                                             mScreenSize.width, mScreenSize.height, // Screen size
                                                             0, 0);                                 // Tile-size (unused)
        GeckoAppShell.sendEventToGecko(event);
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

    private boolean handleDirectTextureChange(boolean hasDirectTexture) {
        if (mTileLayer != null) {
            return false;
        }

        Log.e(LOGTAG, "### Creating virtual layer");
        VirtualLayer virtualLayer = new VirtualLayer();
        virtualLayer.setListener(this);
        virtualLayer.setSize(getBufferSize());
        mLayerController.setRoot(virtualLayer);
        mTileLayer = virtualLayer;

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

    private void adjustViewportWithThrottling() {
        if (!mLayerController.getRedrawHint())
            return;

        if (mPendingViewportAdjust)
            return;

        long timeDelta = System.currentTimeMillis() - mLastViewportChangeTime;
        if (timeDelta < MIN_VIEWPORT_CHANGE_DELAY) {
            mLayerController.getView().postDelayed(
                new Runnable() {
                    public void run() {
                        mPendingViewportAdjust = false;
                        adjustViewport();
                    }
                }, MIN_VIEWPORT_CHANGE_DELAY - timeDelta);
            mPendingViewportAdjust = true;
            return;
        }

        adjustViewport();
    }

    void viewportSizeChanged() {
        mViewportSizeChanged = true;
    }

    private void adjustViewport() {
        ViewportMetrics viewportMetrics =
            new ViewportMetrics(mLayerController.getViewportMetrics());

        viewportMetrics.setViewport(viewportMetrics.getClampedViewport());

        GeckoAppShell.sendEventToGecko(GeckoEvent.createViewportEvent(viewportMetrics));
        if (mViewportSizeChanged) {
            mViewportSizeChanged = false;
            GeckoAppShell.viewSizeChanged();
        }

        mLastViewportChangeTime = System.currentTimeMillis();
    }

    /** Implementation of GeckoEventListener. */
    public void handleMessage(String event, JSONObject message) {
        if ("Viewport:UpdateAndDraw".equals(event)) {
            Log.e(LOGTAG, "### Java side Viewport:UpdateAndDraw()!");
            mUpdateViewportOnEndDraw = true;

            // Redraw everything.
            Rect rect = new Rect(0, 0, mBufferSize.width, mBufferSize.height);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createDrawEvent(rect));
        } else if ("Viewport:UpdateLater".equals(event)) {
            Log.e(LOGTAG, "### Java side Viewport:UpdateLater()!");
            mUpdateViewportOnEndDraw = true;
        }
    }

    void geometryChanged() {
        /* Let Gecko know if the screensize has changed */
        sendResizeEventIfNecessary(false);
        adjustViewportWithThrottling();
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
        Log.e(LOGTAG, "### getViewTransform()");

        // NB: We don't begin a transaction here because this can be called in a synchronous
        // manner between beginDrawing() and endDrawing(), and that will cause a deadlock.

        synchronized (mLayerController) {
            ViewportMetrics viewportMetrics = mLayerController.getViewportMetrics();
            PointF viewportOrigin = viewportMetrics.getOrigin();
            Point tileOrigin = mTileLayer.getOrigin();
            float scrollX = viewportOrigin.x; 
            float scrollY = viewportOrigin.y;
            float zoomFactor = viewportMetrics.getZoomFactor();
            Log.e(LOGTAG, "### Viewport metrics = " + viewportMetrics + " tile reso = " +
                  mTileLayer.getResolution());
            return new ViewTransform(scrollX, scrollY, zoomFactor);
        }
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public LayerRenderer.Frame createFrame() {
        // Create the shaders and textures if necessary.
        if (!mLayerRendererInitialized) {
            mLayerRenderer.createProgram();
            mLayerRendererInitialized = true;
        }

        // Build the contexts and create the frame.
        Layer.RenderContext pageContext = mLayerRenderer.createPageContext();
        Layer.RenderContext screenContext = mLayerRenderer.createScreenContext();
        return mLayerRenderer.createFrame(pageContext, screenContext);
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void activateProgram() {
        mLayerRenderer.activateProgram();
    }

    /** This function is invoked by Gecko via JNI; be careful when modifying signature. */
    public void deactivateProgram() {
        mLayerRenderer.deactivateProgram();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void renderRequested() {
        Log.e(LOGTAG, "### Render requested, scheduling composite");
        GeckoAppShell.scheduleComposite();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void compositionPauseRequested() {
        Log.e(LOGTAG, "### Scheduling PauseComposition");
        GeckoAppShell.schedulePauseComposition();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void compositionResumeRequested() {
        Log.e(LOGTAG, "### Scheduling ResumeComposition");
        GeckoAppShell.scheduleResumeComposition();
    }

    /** Implementation of FlexibleGLSurfaceView.Listener */
    public void surfaceChanged(int width, int height) {
        compositionPauseRequested();
        mLayerController.setViewportSize(new FloatSize(width, height));
        compositionResumeRequested();
        renderRequested();
    }

    /** Implementation of VirtualLayer.Listener */
    public void dimensionsChanged(Point newOrigin, float newResolution) {
        Log.e(LOGTAG, "### dimensionsChanged " + newOrigin + " " + newResolution);
    }

    /** Used by robocop for testing purposes. Not for production use! This is called via reflection by robocop. */
    public void setDrawListener(DrawListener listener) {
        mDrawListener = listener;
    }

    /** Used by robocop for testing purposes. Not for production use! This is used via reflection by robocop. */
    public interface DrawListener {
        public void drawFinished(int x, int y, int width, int height);
    }
}

