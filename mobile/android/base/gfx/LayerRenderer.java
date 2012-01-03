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

import org.mozilla.gecko.gfx.BufferedCairoImage;
import org.mozilla.gecko.gfx.IntSize;
import org.mozilla.gecko.gfx.Layer.RenderContext;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.NinePatchTileLayer;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.gfx.TextureReaper;
import org.mozilla.gecko.gfx.TextLayer;
import org.mozilla.gecko.gfx.TileLayer;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLSurfaceView;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import java.nio.ByteBuffer;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer implements GLSurfaceView.Renderer {
    private static final String LOGTAG = "GeckoLayerRenderer";

    private static final float BACKGROUND_COLOR_R = 0.81f;
    private static final float BACKGROUND_COLOR_G = 0.81f;
    private static final float BACKGROUND_COLOR_B = 0.81f;

    /*
     * The amount of time a frame is allowed to take to render before we declare it a dropped
     * frame.
     */
    private static final int MAX_FRAME_TIME = 16;   /* 1000 ms / 60 FPS */

    private static final int FRAME_RATE_METER_WIDTH = 64;
    private static final int FRAME_RATE_METER_HEIGHT = 32;

    private final LayerView mView;
    private final SingleTileLayer mCheckerboardLayer;
    private final NinePatchTileLayer mShadowLayer;
    private final TextLayer mFrameRateLayer;
    private final ScrollbarLayer mHorizScrollLayer;
    private final ScrollbarLayer mVertScrollLayer;
    private final FadeRunnable mFadeRunnable;
    private RenderContext mLastPageContext;
    private int mMaxTextureSize;

    // Dropped frames display
    private int[] mFrameTimings;
    private int mCurrentFrame, mFrameTimingsSum, mDroppedFrames;
    private boolean mShowFrameRate;

    public LayerRenderer(LayerView view) {
        mView = view;

        LayerController controller = view.getController();

        CairoImage checkerboardImage = new BufferedCairoImage(controller.getCheckerboardPattern());
        mCheckerboardLayer = new SingleTileLayer(true, checkerboardImage);

        CairoImage shadowImage = new BufferedCairoImage(controller.getShadowPattern());
        mShadowLayer = new NinePatchTileLayer(shadowImage);

        IntSize frameRateLayerSize = new IntSize(FRAME_RATE_METER_WIDTH, FRAME_RATE_METER_HEIGHT);
        mFrameRateLayer = TextLayer.create(frameRateLayerSize, "-- ms/--");

        mHorizScrollLayer = ScrollbarLayer.create(false);
        mVertScrollLayer = ScrollbarLayer.create(true);
        mFadeRunnable = new FadeRunnable();

        mFrameTimings = new int[60];
        mCurrentFrame = mFrameTimingsSum = mDroppedFrames = 0;
        mShowFrameRate = false;
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        checkFrameRateMonitorEnabled();

        gl.glHint(GL10.GL_PERSPECTIVE_CORRECTION_HINT, GL10.GL_FASTEST);
        gl.glDisable(GL10.GL_DITHER);
        gl.glEnable(GL10.GL_TEXTURE_2D);

        int maxTextureSizeResult[] = new int[1];
        gl.glGetIntegerv(GL10.GL_MAX_TEXTURE_SIZE, maxTextureSizeResult, 0);
        mMaxTextureSize = maxTextureSizeResult[0];
    }

    public int getMaxTextureSize() {
        return mMaxTextureSize;
    }

    /**
     * Called whenever a new frame is about to be drawn.
     */
    public void onDrawFrame(GL10 gl) {
        long frameStartTime = SystemClock.uptimeMillis();

        TextureReaper.get().reap(gl);

        LayerController controller = mView.getController();
        RenderContext screenContext = createScreenContext();

        synchronized (controller) {
            Layer rootLayer = controller.getRoot();
            RenderContext pageContext = createPageContext();

            if (!pageContext.fuzzyEquals(mLastPageContext)) {
                // the viewport or page changed, so show the scrollbars again
                // as per UX decision
                mVertScrollLayer.unfade();
                mHorizScrollLayer.unfade();
                mFadeRunnable.scheduleStartFade(ScrollbarLayer.FADE_DELAY);
            } else if (mFadeRunnable.timeToFade()) {
                boolean stillFading = mVertScrollLayer.fade() | mHorizScrollLayer.fade();
                if (stillFading) {
                    mFadeRunnable.scheduleNextFadeFrame();
                }
            }
            mLastPageContext = pageContext;

            /* Update layers. */
            if (rootLayer != null) rootLayer.update(gl);
            mShadowLayer.update(gl);
            mCheckerboardLayer.update(gl);
            mFrameRateLayer.update(gl);
            mVertScrollLayer.update(gl);
            mHorizScrollLayer.update(gl);

            /* Draw the background. */
            gl.glClearColor(BACKGROUND_COLOR_R, BACKGROUND_COLOR_G, BACKGROUND_COLOR_B, 1.0f);
            gl.glClear(GL10.GL_COLOR_BUFFER_BIT);

            /* Draw the drop shadow, if we need to. */
            Rect pageRect = getPageRect();
            RectF untransformedPageRect = new RectF(0.0f, 0.0f, pageRect.width(),
                                                    pageRect.height());
            if (!untransformedPageRect.contains(controller.getViewport()))
                mShadowLayer.draw(pageContext);

            /* Draw the checkerboard. */
            Rect scissorRect = transformToScissorRect(pageRect);
            gl.glEnable(GL10.GL_SCISSOR_TEST);
            gl.glScissor(scissorRect.left, scissorRect.top,
                         scissorRect.width(), scissorRect.height());

            mCheckerboardLayer.draw(screenContext);

            /* Draw the layer the client added to us. */
            if (rootLayer != null)
                rootLayer.draw(pageContext);

            gl.glDisable(GL10.GL_SCISSOR_TEST);

            /* Draw the vertical scrollbar. */
            IntSize screenSize = new IntSize(controller.getViewportSize());
            if (pageRect.height() > screenSize.height)
                mVertScrollLayer.draw(pageContext);

            /* Draw the horizontal scrollbar. */
            if (pageRect.width() > screenSize.width)
                mHorizScrollLayer.draw(pageContext);
        }

        /* Draw the FPS. */
        if (mShowFrameRate) {
            updateDroppedFrames(frameStartTime);
            try {
                gl.glEnable(GL10.GL_BLEND);
                gl.glBlendFunc(GL10.GL_SRC_ALPHA, GL10.GL_ONE_MINUS_SRC_ALPHA);
                mFrameRateLayer.draw(screenContext);
            } finally {
                gl.glDisable(GL10.GL_BLEND);
            }
        }

        PanningPerfAPI.recordFrameTime();
    }

    private RenderContext createScreenContext() {
        LayerController layerController = mView.getController();
        IntSize viewportSize = new IntSize(layerController.getViewportSize());
        RectF viewport = new RectF(0.0f, 0.0f, viewportSize.width, viewportSize.height);
        FloatSize pageSize = new FloatSize(layerController.getPageSize());
        return new RenderContext(viewport, pageSize, 1.0f);
    }

    private RenderContext createPageContext() {
        LayerController layerController = mView.getController();

        Rect viewport = new Rect();
        layerController.getViewport().round(viewport);

        FloatSize pageSize = new FloatSize(layerController.getPageSize());
        float zoomFactor = layerController.getZoomFactor();
        return new RenderContext(new RectF(viewport), pageSize, zoomFactor);
    }

    private Rect getPageRect() {
        LayerController controller = mView.getController();

        Point origin = PointUtils.round(controller.getOrigin());
        IntSize pageSize = new IntSize(controller.getPageSize());

        origin.negate();

        return new Rect(origin.x, origin.y,
                        origin.x + pageSize.width, origin.y + pageSize.height);
    }

    private Rect transformToScissorRect(Rect rect) {
        LayerController controller = mView.getController();
        IntSize screenSize = new IntSize(controller.getViewportSize());

        int left = Math.max(0, rect.left);
        int top = Math.max(0, rect.top);
        int right = Math.min(screenSize.width, rect.right);
        int bottom = Math.min(screenSize.height, rect.bottom);

        return new Rect(left, screenSize.height - bottom, right,
                        (screenSize.height - bottom) + (bottom - top));
    }

    public void onSurfaceChanged(GL10 gl, final int width, final int height) {
        gl.glViewport(0, 0, width, height);

        // updating the state in the view/controller/client should be
        // done on the main UI thread, not the GL renderer thread
        mView.post(new Runnable() {
            public void run() {
                mView.setViewportSize(new IntSize(width, height));
                moveFrameRateLayer(width, height);
            }
        });

        /* TODO: Throw away tile images? */
    }

    private void updateDroppedFrames(long frameStartTime) {
        int frameElapsedTime = (int)(SystemClock.uptimeMillis() - frameStartTime);

        /* Update the running statistics. */
        mFrameTimingsSum -= mFrameTimings[mCurrentFrame];
        mFrameTimingsSum += frameElapsedTime;
        mDroppedFrames -= (mFrameTimings[mCurrentFrame] + 1) / MAX_FRAME_TIME;
        mDroppedFrames += (frameElapsedTime + 1) / MAX_FRAME_TIME;

        mFrameTimings[mCurrentFrame] = (int)frameElapsedTime;
        mCurrentFrame = (mCurrentFrame + 1) % mFrameTimings.length;

        int averageTime = mFrameTimingsSum / mFrameTimings.length;

        mFrameRateLayer.beginTransaction();
        try {
            mFrameRateLayer.setText(averageTime + " ms/" + mDroppedFrames);
        } finally {
            mFrameRateLayer.endTransaction();
        }
    }

    /* Given the new dimensions for the surface, moves the frame rate layer appropriately. */
    private void moveFrameRateLayer(int width, int height) {
        mFrameRateLayer.beginTransaction();
        try {
            Point origin = new Point(width - FRAME_RATE_METER_WIDTH - 8,
                                     height - FRAME_RATE_METER_HEIGHT + 8);
            mFrameRateLayer.setOrigin(origin);
        } finally {
            mFrameRateLayer.endTransaction();
        }
    }

    private void checkFrameRateMonitorEnabled() {
        /* Do this I/O off the main thread to minimize its impact on startup time. */
        new Thread(new Runnable() {
            @Override
            public void run() {
                Context context = mView.getContext();
                SharedPreferences preferences = context.getSharedPreferences("GeckoApp", 0);
                mShowFrameRate = preferences.getBoolean("showFrameRate", false);
            }
        }).start();
    }

    class FadeRunnable implements Runnable {
        private boolean mStarted;
        private long mRunAt;

        void scheduleStartFade(long delay) {
            mRunAt = SystemClock.elapsedRealtime() + delay;
            if (!mStarted) {
                mView.postDelayed(this, delay);
                mStarted = true;
            }
        }

        void scheduleNextFadeFrame() {
            if (mStarted) {
                Log.e(LOGTAG, "scheduleNextFadeFrame() called while scheduled for starting fade");
            }
            mView.postDelayed(this, 1000L / 60L); // request another frame at 60fps
        }

        boolean timeToFade() {
            return !mStarted;
        }

        public void run() {
            long timeDelta = mRunAt - SystemClock.elapsedRealtime();
            if (timeDelta > 0) {
                // the run-at time was pushed back, so reschedule
                mView.postDelayed(this, timeDelta);
            } else {
                // reached the run-at time, execute
                mStarted = false;
                mView.requestRender();
            }
        }
    }
}
