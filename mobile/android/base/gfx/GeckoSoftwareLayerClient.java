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

import org.mozilla.gecko.gfx.CairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.LayerClient;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerRenderer;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.ui.ViewportController;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import android.content.Context;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import java.nio.ByteBuffer;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Transfers a software-rendered Gecko to an ImageLayer so that it can be rendered by our
 * compositor.
 *
 * TODO: Throttle down Gecko's priority when we pan and zoom.
 */
public class GeckoSoftwareLayerClient extends LayerClient {
    private Context mContext;
    private int mWidth, mHeight, mFormat;
    private ByteBuffer mBuffer;
    private final SingleTileLayer mTileLayer;
    private ViewportController mViewportController;

    /* The viewport rect that Gecko is currently displaying. */
    private RectF mGeckoVisibleRect;

    private CairoImage mCairoImage;

    private static final long MIN_VIEWPORT_CHANGE_DELAY = 350L;
    private long mLastViewportChangeTime;
    private Timer mViewportRedrawTimer;

    /* The initial page width and height that we use before a page is loaded. */
    private static final int PAGE_WIDTH = 980;      /* Matches MobileSafari. */
    private static final int PAGE_HEIGHT = 1500;

    public GeckoSoftwareLayerClient(Context context) {
        mContext = context;

        mViewportController = new ViewportController(new IntSize(PAGE_WIDTH, PAGE_HEIGHT),
                                                     new RectF(0, 0, 1, 1));

        mWidth = LayerController.TILE_WIDTH;
        mHeight = LayerController.TILE_HEIGHT;
        mFormat = CairoImage.FORMAT_RGB16_565;

        mBuffer = ByteBuffer.allocateDirect(mWidth * mHeight * 2);

        mCairoImage = new CairoImage() {
            @Override
            public ByteBuffer getBuffer() { return mBuffer; }
            @Override
            public int getWidth() { return mWidth; }
            @Override
            public int getHeight() { return mHeight; }
            @Override
            public int getFormat() { return mFormat; }
        };

        mTileLayer = new SingleTileLayer(mCairoImage);
    }

    /** Attaches the root layer to the layer controller so that Gecko appears. */
    @Override
    public void init() {
        getLayerController().setRoot(mTileLayer);
        render();
    }

    public void beginDrawing() {
        mTileLayer.beginTransaction();
    }

    /*
     * TODO: Would be cleaner if this took an android.graphics.Rect instead, but that would require
     * a little more JNI magic.
     */
    public void endDrawing(int x, int y, int width, int height, String metadata) {
        try {
            LayerController controller = getLayerController();
            controller.notifyViewOfGeometryChange();

            try {
                JSONObject metadataObject = new JSONObject(metadata);
                float originX = (float)metadataObject.getDouble("x");
                float originY = (float)metadataObject.getDouble("y");
                mTileLayer.setOrigin(new PointF(originX, originY));
            } catch (JSONException e) {
                throw new RuntimeException(e);
            }

            Rect rect = new Rect(x, y, x + width, y + height);
            mTileLayer.invalidate(rect);
        } finally {
            mTileLayer.endTransaction();
        }
    }

    /** Returns the back buffer. This function is for Gecko to use. */
    public ByteBuffer lockBuffer() {
        return mBuffer;
    }

    /**
     * Gecko calls this function to signal that it is done with the back buffer. After this call,
     * it is forbidden for Gecko to touch the buffer.
     */
    public void unlockBuffer() {
        /* no-op */
    }

    /** Called whenever the page changes size. */
    public void setPageSize(IntSize pageSize) {
        mViewportController.setPageSize(pageSize);
        getLayerController().setPageSize(pageSize);
    }

    @Override
    public void geometryChanged() {
        mViewportController.setVisibleRect(getTransformedVisibleRect());
        render();
    }

    @Override
    public IntSize getPageSize() { return mViewportController.getPageSize(); }

    @Override
    public void render() {
        adjustViewportWithThrottling();
    }

    private void adjustViewportWithThrottling() {
        if (!getLayerController().getRedrawHint())
            return;

        if (System.currentTimeMillis() < mLastViewportChangeTime + MIN_VIEWPORT_CHANGE_DELAY) {
            if (mViewportRedrawTimer != null)
                return;

            mViewportRedrawTimer = new Timer();
            mViewportRedrawTimer.schedule(new TimerTask() {
                @Override
                public void run() {
                    /* We jump back to the UI thread to avoid possible races here. */
                    getLayerController().getView().post(new Runnable() {
                        @Override
                        public void run() {
                            mViewportRedrawTimer = null;
                            adjustViewportWithThrottling();
                        }
                    });
                }
            }, MIN_VIEWPORT_CHANGE_DELAY);
            return;
        }

        adjustViewport();
    }

    private void adjustViewport() {
        LayerController layerController = getLayerController();
        RectF visibleRect = layerController.getVisibleRect();
        RectF tileRect = mViewportController.widenRect(visibleRect);
        tileRect = mViewportController.clampRect(tileRect);

        int x = (int)Math.round(tileRect.left), y = (int)Math.round(tileRect.top);
        GeckoEvent event = new GeckoEvent("Viewport:Change", "{\"x\": " + x +
                                          ", \"y\": " + y + "}");
        GeckoAppShell.sendEventToGecko(event);

        mLastViewportChangeTime = System.currentTimeMillis();
    }

    /* Returns the dimensions of the box in page coordinates that the user is viewing. */
    private RectF getTransformedVisibleRect() {
        LayerController layerController = getLayerController();
        return mViewportController.transformVisibleRect(layerController.getVisibleRect(),
                                                        layerController.getPageSize());
    }
}

