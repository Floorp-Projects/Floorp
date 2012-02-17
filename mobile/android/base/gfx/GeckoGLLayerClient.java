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

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import android.view.View;

public class GeckoGLLayerClient extends GeckoLayerClient
                                implements FlexibleGLSurfaceView.Listener, VirtualLayer.Listener {
    private static final String LOGTAG = "GeckoGLLayerClient";

    private LayerRenderer mLayerRenderer;
    private boolean mLayerRendererInitialized;

    public GeckoGLLayerClient(Context context) {
        super(context);
    }

    @Override
    public Rect beginDrawing(int width, int height, int tileWidth, int tileHeight,
                             String metadata, boolean hasDirectTexture) {
        Rect bufferRect = super.beginDrawing(width, height, tileWidth, tileHeight,
                                             metadata, hasDirectTexture);
        if (bufferRect == null) {
            return null;
        }

        // Be sure to adjust the buffer size; if it's not at least as large as the viewport size,
        // ViewportMetrics.getOptimumViewportOffset() gets awfully confused and severe display
        // corruption results!
        if (mBufferSize.width != width || mBufferSize.height != height) {
            mBufferSize = new IntSize(width, height);
        }

        return bufferRect;
    }

    @Override
    protected boolean handleDirectTextureChange(boolean hasDirectTexture) {
        Log.e(LOGTAG, "### handleDirectTextureChange");
        if (mTileLayer != null) {
            return false;
        }

        Log.e(LOGTAG, "### Creating virtual layer");
        VirtualLayer virtualLayer = new VirtualLayer();
        virtualLayer.setListener(this);
        virtualLayer.setSize(getBufferSize());
        getLayerController().setRoot(virtualLayer);
        mTileLayer = virtualLayer;

        sendResizeEventIfNecessary(true);
        return true;
    }

    @Override
    public void setLayerController(LayerController layerController) {
        super.setLayerController(layerController);

        LayerView view = layerController.getView();
        view.setListener(this);

        mLayerRenderer = new LayerRenderer(view);
    }

    @Override
    protected boolean shouldDrawProceed(int tileWidth, int tileHeight) {
        Log.e(LOGTAG, "### shouldDrawProceed");
        // Always draw.
        return true;
    }

    @Override
    protected void updateLayerAfterDraw(Rect updatedRect) {
        Log.e(LOGTAG, "### updateLayerAfterDraw");
        // Nothing to do.
    }

    @Override
    protected IntSize getBufferSize() {
        View view = (View)getLayerController().getView();
        IntSize size = new IntSize(view.getWidth(), view.getHeight());
        Log.e(LOGTAG, "### getBufferSize " + size);
        return size;
    }

    @Override
    protected IntSize getTileSize() {
        Log.e(LOGTAG, "### getTileSize " + getBufferSize());
        return getBufferSize();
    }

    @Override
    protected void tileLayerUpdated() {
        // Set the new origin and resolution instantly.
        mTileLayer.performUpdates(null);
    }

    @Override
    public Bitmap getBitmap() {
        Log.e(LOGTAG, "### getBitmap");
        IntSize size = getBufferSize();
        try {
            return Bitmap.createBitmap(size.width, size.height, Bitmap.Config.RGB_565);
        } catch (OutOfMemoryError oom) {
            Log.e(LOGTAG, "Unable to create bitmap", oom);
            return null;
        }
    }

    @Override
    public int getType() {
        Log.e(LOGTAG, "### getType");
        return LAYER_CLIENT_TYPE_GL;
    }

    public void dimensionsChanged(Point newOrigin, float newResolution) {
        Log.e(LOGTAG, "### dimensionsChanged " + newOrigin + " " + newResolution);
    }

    /* Informs Gecko that the screen size has changed. */
    @Override
    protected void sendResizeEventIfNecessary(boolean force) {
        Log.e(LOGTAG, "### sendResizeEventIfNecessary " + force);

        IntSize newSize = getBufferSize();
        if (!force && mScreenSize != null && mScreenSize.equals(newSize)) {
            return;
        }

        mScreenSize = newSize;

        Log.e(LOGTAG, "### Screen-size changed to " + mScreenSize);
        GeckoEvent event = GeckoEvent.createSizeChangedEvent(mScreenSize.width, mScreenSize.height,
                                                             mScreenSize.width, mScreenSize.height,
                                                             mScreenSize.width, mScreenSize.height);
        GeckoAppShell.sendEventToGecko(event);
    }

    /** For Gecko to use. */
    public ViewTransform getViewTransform() {
        Log.e(LOGTAG, "### getViewTransform()");

        // NB: We don't begin a transaction here because this can be called in a synchronous
        // manner between beginDrawing() and endDrawing(), and that will cause a deadlock.

        LayerController layerController = getLayerController();
        synchronized (layerController) {
            ViewportMetrics viewportMetrics = layerController.getViewportMetrics();
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

    public void renderRequested() {
        Log.e(LOGTAG, "### Render requested, scheduling composite");
        GeckoAppShell.scheduleComposite();
    }

    public void compositionPauseRequested() {
        Log.e(LOGTAG, "### Scheduling PauseComposition");
        GeckoAppShell.schedulePauseComposition();
    }

    public void compositionResumeRequested() {
        Log.e(LOGTAG, "### Scheduling ResumeComposition");
        GeckoAppShell.scheduleResumeComposition();
    }

    public void surfaceChanged(int width, int height) {
        compositionPauseRequested();
        LayerController layerController = getLayerController();
        layerController.setViewportSize(new FloatSize(width, height));
        compositionResumeRequested();
        renderRequested();
    }

    /** For Gecko to use. */
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

    /** For Gecko to use. */
    public void activateProgram() {
        mLayerRenderer.activateProgram();
    }

    /** For Gecko to use. */
    public void deactivateProgram() {
        mLayerRenderer.deactivateProgram();
    }
}

