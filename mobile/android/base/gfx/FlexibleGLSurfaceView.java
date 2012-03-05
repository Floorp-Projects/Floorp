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
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
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

import org.mozilla.gecko.GeckoApp;
import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class FlexibleGLSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    private static final String LOGTAG = "GeckoFlexibleGLSurfaceView";

    private GLSurfaceView.Renderer mRenderer;
    private GLThread mGLThread; // Protected by this class's monitor.
    private GLController mController;
    private Listener mListener;

    public FlexibleGLSurfaceView(Context context) {
        super(context);
        init();
    }

    public FlexibleGLSurfaceView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        init();
    }

    public void init() {
        SurfaceHolder holder = getHolder();
        holder.addCallback(this);
        holder.setFormat(PixelFormat.RGB_565);

        mController = new GLController(this);
    }

    public void setRenderer(GLSurfaceView.Renderer renderer) {
        mRenderer = renderer;
    }

    public GLSurfaceView.Renderer getRenderer() {
        return mRenderer;
    }

    public void setListener(Listener listener) {
        mListener = listener;
    }

    public synchronized void requestRender() {
        if (mGLThread != null) {
            mGLThread.renderFrame();
        }
        if (mListener != null) {
            mListener.renderRequested();
        }
    }

    /**
     * Creates a Java GL thread. After this is called, the FlexibleGLSurfaceView may be used just
     * like a GLSurfaceView. It is illegal to access the controller after this has been called.
     */
    public synchronized void createGLThread() {
        if (mGLThread != null) {
            throw new FlexibleGLSurfaceViewException("createGLThread() called with a GL thread " +
                                                     "already in place!");
        }

        mGLThread = new GLThread(mController);
        mGLThread.start();
        notifyAll();
    }

    /**
     * Destroys the Java GL thread. Returns a Thread that completes when the Java GL thread is
     * fully shut down.
     */
    public synchronized Thread destroyGLThread() {
        // Wait for the GL thread to be started.
        while (mGLThread == null) {
            try {
                wait();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        Thread glThread = mGLThread;
        mGLThread.shutdown();
        mGLThread = null;
        return glThread;
    }

    public synchronized void recreateSurface() {
        if (mGLThread == null) {
            throw new FlexibleGLSurfaceViewException("recreateSurface() called with no GL " +
                                                     "thread active!");
        }

        mGLThread.recreateSurface();
    }

    public synchronized GLController getGLController() {
        if (mGLThread != null) {
            throw new FlexibleGLSurfaceViewException("getGLController() called with a GL thread " +
                                                     "active; shut down the GL thread first!");
        }

        return mController;
    }

    public synchronized void surfaceChanged(SurfaceHolder holder, int format, int width,
                                            int height) {
        mController.sizeChanged(width, height);
        if (mGLThread != null) {
            mGLThread.surfaceChanged(width, height);
        }
        
        if (mListener != null) {
            mListener.surfaceChanged(width, height);
        }
    }

    public synchronized void surfaceCreated(SurfaceHolder holder) {
        mController.surfaceCreated();
        if (mGLThread != null) {
            mGLThread.surfaceCreated();
        }
    }

    public synchronized void surfaceDestroyed(SurfaceHolder holder) {
        mController.surfaceDestroyed();
        if (mGLThread != null) {
            mGLThread.surfaceDestroyed();
        }
        
        if (mListener != null) {
            mListener.compositionPauseRequested();
        }
    }

    // Called from the compositor thread
    public static GLController registerCxxCompositor() {
        try {
            FlexibleGLSurfaceView flexView = (FlexibleGLSurfaceView)GeckoApp.mAppContext.getLayerController().getView();
            try {
                flexView.destroyGLThread().join();
            } catch (InterruptedException e) {}
            return flexView.getGLController();
        } catch (Exception e) {
            Log.e(LOGTAG, "### Exception! " + e);
            return null;
        }
    }

    public interface Listener {
        void renderRequested();
        void compositionPauseRequested();
        void compositionResumeRequested();
        void surfaceChanged(int width, int height);
    }

    public static class FlexibleGLSurfaceViewException extends RuntimeException {
        public static final long serialVersionUID = 1L;

        FlexibleGLSurfaceViewException(String e) {
            super(e);
        }
    }
}

