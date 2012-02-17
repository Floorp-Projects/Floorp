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

public class GeckoGLLayerClient extends GeckoLayerClient {
    private static final String LOGTAG = "GeckoGLLayerClient";

    private boolean mLayerRendererInitialized;

    public GeckoGLLayerClient(Context context) {
        super(context);
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
}

