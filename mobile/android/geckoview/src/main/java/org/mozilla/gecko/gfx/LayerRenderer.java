/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.os.SystemClock;
import android.util.Log;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.ArrayList;
import java.util.List;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer {
    private static final String LOGTAG = "GeckoLayerRenderer";

    /*
     * The amount of time a frame is allowed to take to render before we declare it a dropped
     * frame.
     */
    private static final int MAX_FRAME_TIME = 16;   /* 1000 ms / 60 FPS */
    private static final long NANOS_PER_MS = 1000000;
    private static final int MAX_SCROLL_SPEED_TO_REQUEST_ZOOM_RENDER = 5;

    private final LayerView mView;
    private ByteBuffer mCoordByteBuffer;
    private FloatBuffer mCoordBuffer;
    private int mMaxTextureSize;

    private long mLastFrameTime;
    private final CopyOnWriteArrayList<RenderTask> mTasks;

    // Dropped frames display
    private final int[] mFrameTimings;
    private int mCurrentFrame, mFrameTimingsSum, mDroppedFrames;

    private IntBuffer mPixelBuffer;
    private List<LayerView.ZoomedViewListener> mZoomedViewListeners;
    private float mLastViewLeft;
    private float mLastViewTop;

    public LayerRenderer(LayerView view) {
        mView = view;

        mTasks = new CopyOnWriteArrayList<RenderTask>();
        mLastFrameTime = System.nanoTime();

        mFrameTimings = new int[60];
        mCurrentFrame = mFrameTimingsSum = mDroppedFrames = 0;

        mZoomedViewListeners = new ArrayList<LayerView.ZoomedViewListener>();
    }

    public void destroy() {
        if (mCoordByteBuffer != null) {
            DirectBufferAllocator.free(mCoordByteBuffer);
            mCoordByteBuffer = null;
            mCoordBuffer = null;
        }
        mZoomedViewListeners.clear();
    }

    void onSurfaceCreated(EGLConfig config) {
        createDefaultProgram();
    }

    public void createDefaultProgram() {
        int maxTextureSizeResult[] = new int[1];
        GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, maxTextureSizeResult, 0);
        mMaxTextureSize = maxTextureSizeResult[0];
    }

    public int getMaxTextureSize() {
        return mMaxTextureSize;
    }

    public void postRenderTask(RenderTask aTask) {
        mTasks.add(aTask);
        mView.requestRender();
    }

    public void removeRenderTask(RenderTask aTask) {
        mTasks.remove(aTask);
    }

    private void runRenderTasks(CopyOnWriteArrayList<RenderTask> tasks, boolean after, long frameStartTime) {
        for (RenderTask task : tasks) {
            if (task.runAfter != after) {
                continue;
            }

            boolean stillRunning = task.run(frameStartTime - mLastFrameTime, frameStartTime);

            // Remove the task from the list if its finished
            if (!stillRunning) {
                tasks.remove(task);
            }
        }
    }

    /** Used by robocop for testing purposes. Not for production use! */
    IntBuffer getPixels() {
        IntBuffer pixelBuffer = IntBuffer.allocate(mView.getWidth() * mView.getHeight());
        synchronized (pixelBuffer) {
            mPixelBuffer = pixelBuffer;
            mView.requestRender();
            try {
                pixelBuffer.wait();
            } catch (InterruptedException ie) {
            }
            mPixelBuffer = null;
        }
        return pixelBuffer;
    }

    private void updateDroppedFrames(long frameStartTime) {
        int frameElapsedTime = (int)((System.nanoTime() - frameStartTime) / NANOS_PER_MS);

        /* Update the running statistics. */
        mFrameTimingsSum -= mFrameTimings[mCurrentFrame];
        mFrameTimingsSum += frameElapsedTime;
        mDroppedFrames -= (mFrameTimings[mCurrentFrame] + 1) / MAX_FRAME_TIME;
        mDroppedFrames += (frameElapsedTime + 1) / MAX_FRAME_TIME;

        mFrameTimings[mCurrentFrame] = frameElapsedTime;
        mCurrentFrame = (mCurrentFrame + 1) % mFrameTimings.length;

        int averageTime = mFrameTimingsSum / mFrameTimings.length;
    }

    public Frame createFrame(ImmutableViewportMetrics metrics) {
        return new Frame(metrics);
    }

    public class Frame {
        // The timestamp recording the start of this frame.
        private long mFrameStartTime;
        // A fixed snapshot of the viewport metrics that this frame is using to render content.
        private final ImmutableViewportMetrics mFrameMetrics;

        public Frame(ImmutableViewportMetrics metrics) {
            mFrameMetrics = metrics;
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapForJNI
        public void beginDrawing() {
            mFrameStartTime = System.nanoTime();

            // Run through pre-render tasks
            runRenderTasks(mTasks, false, mFrameStartTime);
        }


        private void maybeRequestZoomedViewRender() {
            // Concurrently update of mZoomedViewListeners should not be an issue here
            // because the following line is just a short-circuit
            if (mZoomedViewListeners.size() == 0) {
                return;
            }

            // When scrolling fast, do not request zoomed view render to avoid to slow down
            // the scroll in the main view.
            // Speed is estimated using the offset changes between 2 display frame calls
            final float viewLeft = Math.round(mFrameMetrics.getViewport().left);
            final float viewTop = Math.round(mFrameMetrics.getViewport().top);
            boolean shouldWaitToRender = false;

            if (Math.abs(mLastViewLeft - viewLeft) > MAX_SCROLL_SPEED_TO_REQUEST_ZOOM_RENDER ||
                Math.abs(mLastViewTop - viewTop) > MAX_SCROLL_SPEED_TO_REQUEST_ZOOM_RENDER) {
                shouldWaitToRender = true;
            }

            mLastViewLeft = viewLeft;
            mLastViewTop = viewTop;

            if (shouldWaitToRender) {
                return;
            }

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    for (LayerView.ZoomedViewListener listener : mZoomedViewListeners) {
                        listener.requestZoomedViewRender();
                    }
                }
            });
        }


        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapForJNI
        public void endDrawing() {
            PanningPerfAPI.recordFrameTime();

            runRenderTasks(mTasks, true, mFrameStartTime);
            maybeRequestZoomedViewRender();

            /* Used by robocop for testing purposes */
            IntBuffer pixelBuffer = mPixelBuffer;
            if (pixelBuffer != null) {
                synchronized (pixelBuffer) {
                    pixelBuffer.position(0);
                    GLES20.glReadPixels(0, 0, Math.round(mFrameMetrics.getWidth()),
                                        Math.round(mFrameMetrics.getHeight()), GLES20.GL_RGBA,
                                        GLES20.GL_UNSIGNED_BYTE, pixelBuffer);
                    pixelBuffer.notify();
                }
            }

            // Remove background color once we've painted. GeckoLayerClient is
            // responsible for setting this flag before current document is
            // composited.
            if (mView.getPaintState() == LayerView.PAINT_BEFORE_FIRST) {
                mView.post(new Runnable() {
                    @Override
                    public void run() {
                        mView.setSurfaceBackgroundColor(Color.TRANSPARENT);
                    }
                });
                mView.setPaintState(LayerView.PAINT_AFTER_FIRST);
            }
            mLastFrameTime = mFrameStartTime;
        }
    }

    public void updateZoomedView(final ByteBuffer data) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                for (LayerView.ZoomedViewListener listener : mZoomedViewListeners) {
                    data.position(0);
                    listener.updateView(data);
                }
            }
        });
    }

    public void addZoomedViewListener(LayerView.ZoomedViewListener listener) {
        ThreadUtils.assertOnUiThread();
        mZoomedViewListeners.add(listener);
    }

    public void removeZoomedViewListener(LayerView.ZoomedViewListener listener) {
        ThreadUtils.assertOnUiThread();
        mZoomedViewListeners.remove(listener);
    }
}
