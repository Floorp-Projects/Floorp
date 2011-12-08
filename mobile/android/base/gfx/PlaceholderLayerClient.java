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

import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.CairoUtils;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.LayerClient;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Environment;
import android.util.Log;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.File;
import java.io.ByteArrayInputStream;
import java.nio.ByteBuffer;

/**
 * A stand-in for Gecko that renders cached content of the previous page. We use this until Gecko
 * is up, then we hand off control to it.
 */
public class PlaceholderLayerClient extends LayerClient {
    private static final String LOGTAG = "PlaceholderLayerClient";

    private Context mContext;
    private ViewportMetrics mViewport;
    private boolean mViewportUnknown;
    private int mWidth, mHeight, mFormat;
    private ByteBuffer mBuffer;

    private PlaceholderLayerClient(Context context) {
        mContext = context;
        String viewport = GeckoApp.mAppContext.getLastViewport();
        mViewportUnknown = true;
        if (viewport != null) {
            try {
                JSONObject viewportObject = new JSONObject(viewport);
                mViewport = new ViewportMetrics(viewportObject);
                mViewportUnknown = false;
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error parsing saved viewport!");
                mViewport = new ViewportMetrics();
            }
        } else {
            mViewport = new ViewportMetrics();
        }
        loadScreenshot();
    }

    public static PlaceholderLayerClient createInstance(Context context) {
        return new PlaceholderLayerClient(context);
    }

    public void destroy() {
        if (mBuffer != null) {
            GeckoAppShell.freeDirectBuffer(mBuffer);
            mBuffer = null;
        }
    }

    boolean loadScreenshot() {
        if (GeckoApp.mAppContext.mLastScreen == null)
            return false;
        Bitmap bitmap = BitmapFactory.decodeStream(new ByteArrayInputStream(GeckoApp.mAppContext.mLastScreen));
        if (bitmap == null)
            return false;

        Bitmap.Config config = bitmap.getConfig();

        mWidth = bitmap.getWidth();
        mHeight = bitmap.getHeight();
        mFormat = CairoUtils.bitmapConfigToCairoFormat(config);

        int bpp = CairoUtils.bitsPerPixelForCairoFormat(mFormat) / 8;
        mBuffer = GeckoAppShell.allocateDirectBuffer(mWidth * mHeight * bpp);

        bitmap.copyPixelsToBuffer(mBuffer.asIntBuffer());

        if (mViewportUnknown) {
            mViewport.setPageSize(new FloatSize(mWidth, mHeight));
            if (getLayerController() != null)
                getLayerController().setPageSize(mViewport.getPageSize());
        }

        return true;
    }

    void showScreenshot() {
        BufferedCairoImage image = new BufferedCairoImage(mBuffer, mWidth, mHeight, mFormat);
        SingleTileLayer tileLayer = new SingleTileLayer(image);

        beginTransaction(tileLayer);
        tileLayer.setOrigin(PointUtils.round(mViewport.getDisplayportOrigin()));
        endTransaction(tileLayer);

        getLayerController().setRoot(tileLayer);
    }

    @Override
    public void geometryChanged() { /* no-op */ }
    @Override
    public void viewportSizeChanged() { /* no-op */ }
    @Override
    public void render() { /* no-op */ }

    @Override
    public void setLayerController(LayerController layerController) {
        super.setLayerController(layerController);

        if (mViewportUnknown)
            mViewport.setViewport(layerController.getViewport());
        layerController.setViewportMetrics(mViewport);
        layerController.notifyPanZoomControllerOfGeometryChange(false);

        BufferedCairoImage image = new BufferedCairoImage(mBuffer, mWidth, mHeight, mFormat);
        SingleTileLayer tileLayer = new SingleTileLayer(image);

        beginTransaction(tileLayer);
        tileLayer.setOrigin(PointUtils.round(mViewport.getDisplayportOrigin()));
        endTransaction(tileLayer);

        layerController.setRoot(tileLayer);
    }
}
