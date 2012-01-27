/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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

package org.mozilla.gecko.gfx.layers;

import android.content.Context;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;
import android.view.View;

import org.mozilla.gecko.gfx.*;
import org.mozilla.gecko.GeckoInputConnection;

public class OGLSurfaceView implements AbstractLayerView {
    private static final String LOGTAG = "OGLSurfaceView";

    // Used by registerCompositor to replace the temporary java compositor
    private static LayerController mLayerController;

    private Context mContext;
    private LayerController mController;
    private InputConnectionHandler mInputConnectionHandler;

    public OGLSurfaceView(Context context, LayerController controller) {
        // Constructed on the main thread
        mContext = context;
        mController = controller;
        System.out.println("construct");
    }

    public SurfaceHolder getHolder() {
        System.out.println("Get Holder");
        return null;
    }

    // Called from the Java thread
    public static void registerLayerController(LayerController layerController) {
        System.out.println("register layer controller");
        synchronized (OGLSurfaceView.class) {
            mLayerController = layerController;
            OGLSurfaceView.class.notifyAll();
        }
    }

    // Called from the compositor thread
    public static void registerCompositor() {
        System.out.println("register layer comp");
        synchronized (OGLSurfaceView.class) {
            // Wait for the layer controller if by some miracle
            // gecko beats the java thread here.
            while (mLayerController == null) {
                try {
                    OGLSurfaceView.class.wait();
                } catch (InterruptedException e) {}
            }
            final LayerController controller = mLayerController;

            GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                public void run() {
                    synchronized (OGLSurfaceView.class) {
                        OGLSurfaceView surfaceView =
                            new OGLSurfaceView(controller.getContext(), controller);
                        OGLSurfaceView.class.notifyAll();
                    }
                }
            });

            // Wait for the compositor to be setup on the
            // Java UI thread.
            try {
                OGLSurfaceView.class.wait();
            } catch (InterruptedException e) {}
        }

    }

    public static native void setSurfaceView(SurfaceView sv);

    public LayerController getController() {
        return mController; 
    }

    public GeckoInputConnection setInputConnectionHandler() {
        GeckoInputConnection geckoInputConnection = GeckoInputConnection.create(getAndroidView());
        mInputConnectionHandler = geckoInputConnection;
        return geckoInputConnection;
    }

    public View getAndroidView() {
        return null;
    }

    /** The LayerRenderer calls this to indicate that the window has changed size. */
    public void setViewportSize(IntSize size) {}
    public void requestRender() {}
    public boolean post(Runnable action) { return false; }
    public boolean postDelayed(Runnable action, long delayMillis) { return false; }
    public Context getContext() { return mContext; }
    public int getMaxTextureSize() { return 1024; }

    private class InternalSurfaceView extends SurfaceView {
        public InternalSurfaceView() {
            super(OGLSurfaceView.this.mContext);
        }
    }
}
