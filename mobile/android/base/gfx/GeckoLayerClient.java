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
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public abstract class GeckoLayerClient extends LayerClient implements GeckoEventListener {
    private static final String LOGTAG = "GeckoLayerClient";

    protected IntSize mScreenSize;
    protected IntSize mBufferSize;

    protected Layer mTileLayer;

    /* The viewport that Gecko is currently displaying. */
    protected ViewportMetrics mGeckoViewport;

    /* The viewport that Gecko will display when drawing is finished */
    protected ViewportMetrics mNewGeckoViewport;

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

    protected abstract boolean handleDirectTextureChange(boolean hasDirectTexture);
    protected abstract boolean shouldDrawProceed(int tileWidth, int tileHeight);
    protected abstract void updateLayerAfterDraw(Rect updatedRect);
    protected abstract IntSize getBufferSize();
    protected abstract IntSize getTileSize();
    protected abstract void tileLayerUpdated();
    public abstract Bitmap getBitmap();

    public GeckoLayerClient(Context context) {
        mScreenSize = new IntSize(0, 0);
        mBufferSize = new IntSize(0, 0);
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    @Override
    public void setLayerController(LayerController layerController) {
        super.setLayerController(layerController);

        layerController.setRoot(mTileLayer);
        if (mGeckoViewport != null) {
            layerController.setViewportMetrics(mGeckoViewport);
        }

        GeckoAppShell.registerGeckoEventListener("Viewport:UpdateAndDraw", this);
        GeckoAppShell.registerGeckoEventListener("Viewport:UpdateLater", this);

        sendResizeEventIfNecessary();
    }

    public Rect beginDrawing(int width, int height, int tileWidth, int tileHeight,
                             String metadata, boolean hasDirectTexture) {
        Log.e(LOGTAG, "### beginDrawing " + width + " " + height + " " + tileWidth + " " +
              tileHeight + " " + hasDirectTexture);

        // If we've changed surface types, cancel this draw
        if (handleDirectTextureChange(hasDirectTexture)) {
            Log.e(LOGTAG, "### Cancelling draw due to direct texture change");
            return null;
        }

        if (!shouldDrawProceed(tileWidth, tileHeight)) {
            Log.e(LOGTAG, "### Cancelling draw due to shouldDrawProceed()");
            return null;
        }

        LayerController controller = getLayerController();

        try {
            JSONObject viewportObject = new JSONObject(metadata);
            mNewGeckoViewport = new ViewportMetrics(viewportObject);

            Log.e(LOGTAG, "### beginDrawing new Gecko viewport " + mNewGeckoViewport);

            // Update the background color, if it's present.
            String backgroundColorString = viewportObject.optString("backgroundColor");
            if (backgroundColorString != null && !backgroundColorString.equals(mLastCheckerboardColor)) {
                mLastCheckerboardColor = backgroundColorString;
                controller.setCheckerboardColor(parseColorFromGecko(backgroundColorString));
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "Aborting draw, bad viewport description: " + metadata);
            return null;
        }


        // Make sure we don't spend time painting areas we aren't interested in.
        // Only do this if the Gecko viewport isn't going to override our viewport.
        Rect bufferRect = new Rect(0, 0, width, height);

        if (!mUpdateViewportOnEndDraw) {
            // First, find out our ideal displayport. We do this by taking the
            // clamped viewport origin and taking away the optimum viewport offset.
            // This would be what we would send to Gecko if adjustViewport were
            // called now.
            ViewportMetrics currentMetrics = controller.getViewportMetrics();
            PointF currentBestOrigin = RectUtils.getOrigin(currentMetrics.getClampedViewport());
            PointF viewportOffset = currentMetrics.getOptimumViewportOffset(new IntSize(width, height));
            currentBestOrigin.offset(-viewportOffset.x, -viewportOffset.y);

            Rect currentRect = RectUtils.round(new RectF(currentBestOrigin.x, currentBestOrigin.y,
                                                         currentBestOrigin.x + width, currentBestOrigin.y + height));

            // Second, store Gecko's displayport.
            PointF currentOrigin = mNewGeckoViewport.getDisplayportOrigin();
            bufferRect = RectUtils.round(new RectF(currentOrigin.x, currentOrigin.y,
                                                   currentOrigin.x + width, currentOrigin.y + height));


            // Take the intersection of the two as the area we're interested in rendering.
            if (!bufferRect.intersect(currentRect)) {
                // If there's no intersection, we have no need to render anything,
                // but make sure to update the viewport size.
                beginTransaction(mTileLayer);
                try {
                    updateViewport(true);
                } finally {
                    endTransaction(mTileLayer);
                }
                return null;
            }
            bufferRect.offset(Math.round(-currentOrigin.x), Math.round(-currentOrigin.y));
        }

        beginTransaction(mTileLayer);
        return bufferRect;
    }

    /*
     * TODO: Would be cleaner if this took an android.graphics.Rect instead, but that would require
     * a little more JNI magic.
     */
    public void endDrawing(int x, int y, int width, int height) {
        synchronized (getLayerController()) {
            try {
                updateViewport(!mUpdateViewportOnEndDraw);
                mUpdateViewportOnEndDraw = false;

                Rect rect = new Rect(x, y, x + width, y + height);
                updateLayerAfterDraw(rect);
            } finally {
                endTransaction(mTileLayer);
            }
        }
        Log.i(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - endDrawing");

        /* Used by robocop for testing purposes */
        if (mDrawListener != null) {
            mDrawListener.drawFinished(x, y, width, height);
        }
    }

    protected void updateViewport(boolean onlyUpdatePageSize) {
        // save and restore the viewport size stored in java; never let the
        // JS-side viewport dimensions override the java-side ones because
        // java is the One True Source of this information, and allowing JS
        // to override can lead to race conditions where this data gets clobbered.
        FloatSize viewportSize = getLayerController().getViewportSize();
        mGeckoViewport = mNewGeckoViewport;
        mGeckoViewport.setSize(viewportSize);

        LayerController controller = getLayerController();
        PointF displayportOrigin = mGeckoViewport.getDisplayportOrigin();
        mTileLayer.setOrigin(PointUtils.round(displayportOrigin));
        mTileLayer.setResolution(mGeckoViewport.getZoomFactor());

        this.tileLayerUpdated();
        Log.e(LOGTAG, "### updateViewport onlyUpdatePageSize=" + onlyUpdatePageSize +
              " getTileViewport " + mGeckoViewport);

        if (onlyUpdatePageSize) {
            // Don't adjust page size when zooming unless zoom levels are
            // approximately equal.
            if (FloatUtils.fuzzyEquals(controller.getZoomFactor(),
                    mGeckoViewport.getZoomFactor()))
                controller.setPageSize(mGeckoViewport.getPageSize());
        } else {
            controller.setViewportMetrics(mGeckoViewport);
            controller.abortPanZoomAnimation();
        }
    }

    /* Informs Gecko that the screen size has changed. */
    protected void sendResizeEventIfNecessary(boolean force) {
        Log.e(LOGTAG, "### sendResizeEventIfNecessary " + force);

        DisplayMetrics metrics = new DisplayMetrics();
        GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

        // Return immediately if the screen size hasn't changed or the viewport
        // size is zero (which indicates that the rendering surface hasn't been
        // allocated yet).
        boolean screenSizeChanged = (metrics.widthPixels != mScreenSize.width ||
                                     metrics.heightPixels != mScreenSize.height);
        boolean viewportSizeValid = (getLayerController() != null &&
                                     getLayerController().getViewportSize().isPositive());
        if (!(force || (screenSizeChanged && viewportSizeValid))) {
            return;
        }

        mScreenSize = new IntSize(metrics.widthPixels, metrics.heightPixels);
        IntSize bufferSize = getBufferSize(), tileSize = getTileSize();

        Log.e(LOGTAG, "### Screen-size changed to " + mScreenSize);
        GeckoEvent event = GeckoEvent.createSizeChangedEvent(bufferSize.width, bufferSize.height,
                                                             metrics.widthPixels, metrics.heightPixels,
                                                             tileSize.width, tileSize.height);
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

    public void render() {
        adjustViewportWithThrottling();
    }

    private void adjustViewportWithThrottling() {
        if (!getLayerController().getRedrawHint())
            return;

        if (mPendingViewportAdjust)
            return;

        long timeDelta = System.currentTimeMillis() - mLastViewportChangeTime;
        if (timeDelta < MIN_VIEWPORT_CHANGE_DELAY) {
            getLayerController().getView().postDelayed(
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

    public void viewportSizeChanged() {
        mViewportSizeChanged = true;
    }

    private void adjustViewport() {
        ViewportMetrics viewportMetrics =
            new ViewportMetrics(getLayerController().getViewportMetrics());

        PointF viewportOffset = viewportMetrics.getOptimumViewportOffset(mBufferSize);
        viewportMetrics.setViewportOffset(viewportOffset);
        viewportMetrics.setViewport(viewportMetrics.getClampedViewport());

        GeckoAppShell.sendEventToGecko(GeckoEvent.createViewportEvent(viewportMetrics));
        if (mViewportSizeChanged) {
            mViewportSizeChanged = false;
            GeckoAppShell.viewSizeChanged();
        }

        mLastViewportChangeTime = System.currentTimeMillis();
    }

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

    public void geometryChanged() {
        /* Let Gecko know if the screensize has changed */
        sendResizeEventIfNecessary();
        render();
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

    private void sendResizeEventIfNecessary() {
        sendResizeEventIfNecessary(false);
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

