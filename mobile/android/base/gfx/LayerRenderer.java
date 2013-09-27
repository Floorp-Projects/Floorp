/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.gfx.Layer.RenderContext;
import org.mozilla.gecko.gfx.RenderTask;
import org.mozilla.gecko.mozglue.DirectBufferAllocator;
import org.mozilla.gecko.mozglue.generatorannotations.GeneratorOptions;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.os.SystemClock;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.concurrent.CopyOnWriteArrayList;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "GeckoLayerRenderer";
    private static final String PROFTAG = "GeckoLayerRendererProf";

    /*
     * The amount of time a frame is allowed to take to render before we declare it a dropped
     * frame.
     */
    private static final int MAX_FRAME_TIME = 16;   /* 1000 ms / 60 FPS */

    private static final int FRAME_RATE_METER_WIDTH = 128;
    private static final int FRAME_RATE_METER_HEIGHT = 32;

    private static final long NANOS_PER_MS = 1000000;
    private static final int NANOS_PER_SECOND = 1000000000;

    private final LayerView mView;
    private final NinePatchTileLayer mShadowLayer;
    private TextLayer mFrameRateLayer;
    private final ScrollbarLayer mHorizScrollLayer;
    private final ScrollbarLayer mVertScrollLayer;
    private final FadeRunnable mFadeRunnable;
    private ByteBuffer mCoordByteBuffer;
    private FloatBuffer mCoordBuffer;
    private RenderContext mLastPageContext;
    private int mMaxTextureSize;
    private int mBackgroundColor;
    private int mOverscrollColor;

    private long mLastFrameTime;
    private final CopyOnWriteArrayList<RenderTask> mTasks;

    private CopyOnWriteArrayList<Layer> mExtraLayers = new CopyOnWriteArrayList<Layer>();

    // Dropped frames display
    private int[] mFrameTimings;
    private int mCurrentFrame, mFrameTimingsSum, mDroppedFrames;

    // Render profiling output
    private int mFramesRendered;
    private float mCompleteFramesRendered;
    private boolean mProfileRender;
    private long mProfileOutputTime;

    /* Used by robocop for testing purposes */
    private IntBuffer mPixelBuffer;

    // Used by GLES 2.0
    private int mProgram;
    private int mPositionHandle;
    private int mTextureHandle;
    private int mSampleHandle;
    private int mTMatrixHandle;

    // column-major matrix applied to each vertex to shift the viewport from
    // one ranging from (-1, -1),(1,1) to (0,0),(1,1) and to scale all sizes by
    // a factor of 2 to fill up the screen
    public static final float[] DEFAULT_TEXTURE_MATRIX = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 2.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f
    };

    private static final int COORD_BUFFER_SIZE = 20;

    // The shaders run on the GPU directly, the vertex shader is only applying the
    // matrix transform detailed above

    // Note we flip the y-coordinate in the vertex shader from a
    // coordinate system with (0,0) in the top left to one with (0,0) in
    // the bottom left.

    public static final String DEFAULT_VERTEX_SHADER =
        "uniform mat4 uTMatrix;\n" +
        "attribute vec4 vPosition;\n" +
        "attribute vec2 aTexCoord;\n" +
        "varying vec2 vTexCoord;\n" +
        "void main() {\n" +
        "    gl_Position = uTMatrix * vPosition;\n" +
        "    vTexCoord.x = aTexCoord.x;\n" +
        "    vTexCoord.y = 1.0 - aTexCoord.y;\n" +
        "}\n";

    // We use highp because the screenshot textures
    // we use are large and we stretch them alot
    // so we need all the precision we can get.
    // Unfortunately, highp is not required by ES 2.0
    // so on GPU's like Mali we end up getting mediump
    public static final String DEFAULT_FRAGMENT_SHADER =
        "precision highp float;\n" +
        "varying vec2 vTexCoord;\n" +
        "uniform sampler2D sTexture;\n" +
        "void main() {\n" +
        "    gl_FragColor = texture2D(sTexture, vTexCoord);\n" +
        "}\n";

    public LayerRenderer(LayerView view) {
        mView = view;
        mOverscrollColor = view.getContext().getResources().getColor(R.color.background_normal);

        CairoImage shadowImage = new BufferedCairoImage(view.getShadowPattern());
        mShadowLayer = new NinePatchTileLayer(shadowImage);

        Bitmap scrollbarImage = view.getScrollbarImage();
        IntSize size = new IntSize(scrollbarImage.getWidth(), scrollbarImage.getHeight());
        scrollbarImage = expandCanvasToPowerOfTwo(scrollbarImage, size);

        mTasks = new CopyOnWriteArrayList<RenderTask>();
        mLastFrameTime = System.nanoTime();

        mVertScrollLayer = new ScrollbarLayer(this, scrollbarImage, size, true);
        mHorizScrollLayer = new ScrollbarLayer(this, diagonalFlip(scrollbarImage), new IntSize(size.height, size.width), false);
        mFadeRunnable = new FadeRunnable();

        mFrameTimings = new int[60];
        mCurrentFrame = mFrameTimingsSum = mDroppedFrames = 0;

        // Initialize the FloatBuffer that will be used to store all vertices and texture
        // coordinates in draw() commands.
        mCoordByteBuffer = DirectBufferAllocator.allocate(COORD_BUFFER_SIZE * 4);
        mCoordByteBuffer.order(ByteOrder.nativeOrder());
        mCoordBuffer = mCoordByteBuffer.asFloatBuffer();

        Tabs.registerOnTabsChangedListener(this);
    }

    private Bitmap expandCanvasToPowerOfTwo(Bitmap image, IntSize size) {
        IntSize potSize = size.nextPowerOfTwo();
        if (size.equals(potSize)) {
            return image;
        }
        // make the bitmap size a power-of-two in both dimensions if it's not already.
        Bitmap potImage = Bitmap.createBitmap(potSize.width, potSize.height, image.getConfig());
        new Canvas(potImage).drawBitmap(image, new Matrix(), null);
        return potImage;
    }

    private Bitmap diagonalFlip(Bitmap image) {
        Matrix rotation = new Matrix();
        rotation.setValues(new float[] { 0, 1, 0, 1, 0, 0, 0, 0, 1 }); // transform (x,y) into (y,x)
        Bitmap rotated = Bitmap.createBitmap(image, 0, 0, image.getWidth(), image.getHeight(), rotation, true);
        return rotated;
    }

    public void destroy() {
        DirectBufferAllocator.free(mCoordByteBuffer);
        mCoordByteBuffer = null;
        mCoordBuffer = null;
        mShadowLayer.destroy();
        mHorizScrollLayer.destroy();
        mVertScrollLayer.destroy();
        if (mFrameRateLayer != null) {
            mFrameRateLayer.destroy();
        }
        Tabs.unregisterOnTabsChangedListener(this);
    }

    void onSurfaceCreated(EGLConfig config) {
        checkMonitoringEnabled();
        createDefaultProgram();
        activateDefaultProgram();
    }

    public void createDefaultProgram() {
        int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, DEFAULT_VERTEX_SHADER);
        int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, DEFAULT_FRAGMENT_SHADER);

        mProgram = GLES20.glCreateProgram();
        GLES20.glAttachShader(mProgram, vertexShader);   // add the vertex shader to program
        GLES20.glAttachShader(mProgram, fragmentShader); // add the fragment shader to program
        GLES20.glLinkProgram(mProgram);                  // creates OpenGL program executables

        // Get handles to the vertex shader's vPosition, aTexCoord, sTexture, and uTMatrix members.
        mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");
        mTextureHandle = GLES20.glGetAttribLocation(mProgram, "aTexCoord");
        mSampleHandle = GLES20.glGetUniformLocation(mProgram, "sTexture");
        mTMatrixHandle = GLES20.glGetUniformLocation(mProgram, "uTMatrix");

        int maxTextureSizeResult[] = new int[1];
        GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, maxTextureSizeResult, 0);
        mMaxTextureSize = maxTextureSizeResult[0];
    }

    // Activates the shader program.
    public void activateDefaultProgram() {
        // Add the program to the OpenGL environment
        GLES20.glUseProgram(mProgram);

        // Set the transformation matrix
        GLES20.glUniformMatrix4fv(mTMatrixHandle, 1, false, DEFAULT_TEXTURE_MATRIX, 0);

        // Enable the arrays from which we get the vertex and texture coordinates
        GLES20.glEnableVertexAttribArray(mPositionHandle);
        GLES20.glEnableVertexAttribArray(mTextureHandle);

        GLES20.glUniform1i(mSampleHandle, 0);

        // TODO: Move these calls into a separate deactivate() call that is called after the
        // underlay and overlay are rendered.
    }

    // Deactivates the shader program. This must be done to avoid crashes after returning to the
    // Gecko C++ compositor from Java.
    public void deactivateDefaultProgram() {
        GLES20.glDisableVertexAttribArray(mTextureHandle);
        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glUseProgram(0);
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

    public void addLayer(Layer layer) {
        synchronized (mExtraLayers) {
            if (mExtraLayers.contains(layer)) {
                mExtraLayers.remove(layer);
            }

            mExtraLayers.add(layer);
        }
    }

    public void removeLayer(Layer layer) {
        synchronized (mExtraLayers) {
            mExtraLayers.remove(layer);
        }
    }

    private void printCheckerboardStats() {
        Log.d(PROFTAG, "Frames rendered over last 1000ms: " + mCompleteFramesRendered + "/" + mFramesRendered);
        mFramesRendered = 0;
        mCompleteFramesRendered = 0;
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

    private RenderContext createScreenContext(ImmutableViewportMetrics metrics, PointF offset) {
        RectF viewport = new RectF(0.0f, 0.0f, metrics.getWidth(), metrics.getHeight());
        RectF pageRect = metrics.getPageRect();

        return createContext(viewport, pageRect, 1.0f, offset);
    }

    private RenderContext createPageContext(ImmutableViewportMetrics metrics, PointF offset) {
        RectF viewport = metrics.getViewport();
        RectF pageRect = metrics.getPageRect();
        float zoomFactor = metrics.zoomFactor;

        return createContext(new RectF(RectUtils.round(viewport)), pageRect, zoomFactor, offset);
    }

    private RenderContext createContext(RectF viewport, RectF pageRect, float zoomFactor, PointF offset) {
        return new RenderContext(viewport, pageRect, zoomFactor, offset, mPositionHandle, mTextureHandle,
                                 mCoordBuffer);
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
        mFrameRateLayer.beginTransaction();     // called on compositor thread
        try {
            mFrameRateLayer.setText(averageTime + " ms/" + mDroppedFrames);
        } finally {
            mFrameRateLayer.endTransaction();
        }
    }

    /* Given the new dimensions for the surface, moves the frame rate layer appropriately. */
    private void moveFrameRateLayer(int width, int height) {
        mFrameRateLayer.beginTransaction();     // called on compositor thread
        try {
            Rect position = new Rect(width - FRAME_RATE_METER_WIDTH - 8,
                                    height - FRAME_RATE_METER_HEIGHT + 8,
                                    width - 8,
                                    height + 8);
            mFrameRateLayer.setPosition(position);
        } finally {
            mFrameRateLayer.endTransaction();
        }
    }

    void checkMonitoringEnabled() {
        /* Do this I/O off the main thread to minimize its impact on startup time. */
        new Thread(new Runnable() {
            @Override
            public void run() {
                Context context = mView.getContext();
                SharedPreferences preferences = context.getSharedPreferences("GeckoApp", 0);
                if (preferences.getBoolean("showFrameRate", false)) {
                    IntSize frameRateLayerSize = new IntSize(FRAME_RATE_METER_WIDTH, FRAME_RATE_METER_HEIGHT);
                    mFrameRateLayer = TextLayer.create(frameRateLayerSize, "-- ms/--");
                    moveFrameRateLayer(mView.getWidth(), mView.getHeight());
                }
                mProfileRender = Log.isLoggable(PROFTAG, Log.DEBUG);
            }
        }).start();
    }

    /*
     * create a vertex shader type (GLES20.GL_VERTEX_SHADER)
     * or a fragment shader type (GLES20.GL_FRAGMENT_SHADER)
     */
    public static int loadShader(int type, String shaderCode) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, shaderCode);
        GLES20.glCompileShader(shader);
        return shader;
    }

    public Frame createFrame(ImmutableViewportMetrics metrics) {
        return new Frame(metrics);
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

        @Override
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

    @GeneratorOptions(generatedClassName = "LayerRendererFrame")
    public class Frame {
        // The timestamp recording the start of this frame.
        private long mFrameStartTime;
        // A fixed snapshot of the viewport metrics that this frame is using to render content.
        private ImmutableViewportMetrics mFrameMetrics;
        // A rendering context for page-positioned layers, and one for screen-positioned layers.
        private RenderContext mPageContext, mScreenContext;
        // Whether a layer was updated.
        private boolean mUpdated;
        private final Rect mPageRect;
        private final Rect mAbsolutePageRect;
        private final PointF mRenderOffset;

        public Frame(ImmutableViewportMetrics metrics) {
            mFrameMetrics = metrics;

            // Work out the offset due to margins
            Layer rootLayer = mView.getLayerClient().getRoot();
            mRenderOffset = mFrameMetrics.getMarginOffset();
            mPageContext = createPageContext(metrics, mRenderOffset);
            mScreenContext = createScreenContext(metrics, mRenderOffset);

            RectF pageRect = mFrameMetrics.getPageRect();
            mAbsolutePageRect = RectUtils.round(pageRect);

            PointF origin = mFrameMetrics.getOrigin();
            pageRect.offset(-origin.x, -origin.y);
            mPageRect = RectUtils.round(pageRect);
        }

        private void setScissorRect() {
            Rect scissorRect = transformToScissorRect(mPageRect);
            GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
            GLES20.glScissor(scissorRect.left, scissorRect.top,
                             scissorRect.width(), scissorRect.height());
        }

        private Rect transformToScissorRect(Rect rect) {
            IntSize screenSize = new IntSize(mFrameMetrics.getSize());

            int left = Math.max(0, rect.left);
            int top = Math.max(0, rect.top);
            int right = Math.min(screenSize.width, rect.right);
            int bottom = Math.min(screenSize.height, rect.bottom);

            Rect scissorRect = new Rect(left, screenSize.height - bottom, right,
                                        (screenSize.height - bottom) + (bottom - top));
            scissorRect.offset(Math.round(-mRenderOffset.x), Math.round(-mRenderOffset.y));

            return scissorRect;
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapElementForJNI(allowMultithread = true)
        public void beginDrawing() {
            mFrameStartTime = System.nanoTime();

            TextureReaper.get().reap();
            TextureGenerator.get().fill();

            mUpdated = true;

            Layer rootLayer = mView.getLayerClient().getRoot();

            // Run through pre-render tasks
            runRenderTasks(mTasks, false, mFrameStartTime);

            if (!mPageContext.fuzzyEquals(mLastPageContext) && !mView.isFullScreen()) {
                // The viewport or page changed, so show the scrollbars again
                // as per UX decision. Don't do this if we're in full-screen mode though.
                mVertScrollLayer.unfade();
                mHorizScrollLayer.unfade();
                mFadeRunnable.scheduleStartFade(ScrollbarLayer.FADE_DELAY);
            } else if (mFadeRunnable.timeToFade()) {
                boolean stillFading = mVertScrollLayer.fade() | mHorizScrollLayer.fade();
                if (stillFading) {
                    mFadeRunnable.scheduleNextFadeFrame();
                }
            }
            mLastPageContext = mPageContext;

            /* Update layers. */
            if (rootLayer != null) mUpdated &= rootLayer.update(mPageContext);  // called on compositor thread
            mUpdated &= mShadowLayer.update(mPageContext);  // called on compositor thread
            if (mFrameRateLayer != null) mUpdated &= mFrameRateLayer.update(mScreenContext); // called on compositor thread
            mUpdated &= mVertScrollLayer.update(mPageContext);  // called on compositor thread
            mUpdated &= mHorizScrollLayer.update(mPageContext); // called on compositor thread

            for (Layer layer : mExtraLayers)
                mUpdated &= layer.update(mPageContext); // called on compositor thread
        }

        /** Retrieves the bounds for the layer, rounded in such a way that it
         * can be used as a mask for something that will render underneath it.
         * This will round the bounds inwards, but stretch the mask towards any
         * near page edge, where near is considered to be 'within 2 pixels'.
         * Returns null if the given layer is null.
         */
        private Rect getMaskForLayer(Layer layer) {
            if (layer == null) {
                return null;
            }

            RectF bounds = RectUtils.contract(layer.getBounds(mPageContext), 1.0f, 1.0f);
            Rect mask = RectUtils.roundIn(bounds);

            // If the mask is within two pixels of any page edge, stretch it over
            // that edge. This is to avoid drawing thin slivers when masking
            // layers.
            if (mask.top <= 2) {
                mask.top = -1;
            }
            if (mask.left <= 2) {
                mask.left = -1;
            }

            // Because we're drawing relative to the page-rect, we only need to
            // take into account its width and height (and not its origin)
            int pageRight = mPageRect.width();
            int pageBottom = mPageRect.height();

            if (mask.right >= pageRight - 2) {
                mask.right = pageRight + 1;
            }
            if (mask.bottom >= pageBottom - 2) {
                mask.bottom = pageBottom + 1;
            }

            return mask;
        }

        private void clear(int color) {
            GLES20.glClearColor(((color >> 16) & 0xFF) / 255.0f,
                                ((color >> 8) & 0xFF) / 255.0f,
                                (color & 0xFF) / 255.0f,
                                0.0f);
            // The bits set here need to match up with those used
            // in gfx/layers/opengl/LayerManagerOGL.cpp.
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT |
                           GLES20.GL_DEPTH_BUFFER_BIT);
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapElementForJNI(allowMultithread = true)
        public void drawBackground() {
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);

            // Draw the overscroll background area as a solid color
            clear(mOverscrollColor);

            // Update background color.
            mBackgroundColor = mView.getBackgroundColor();

            // Clear the page area to the page background colour.
            setScissorRect();
            clear(mBackgroundColor);
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);

            // Draw the drop shadow, if we need to.
            RectF offsetAbsPageRect = new RectF(mAbsolutePageRect);
            offsetAbsPageRect.offset(mRenderOffset.x, mRenderOffset.y);
            if (!offsetAbsPageRect.contains(mFrameMetrics.getViewport()))
                mShadowLayer.draw(mPageContext);
        }

        // Draws the layer the client added to us.
        void drawRootLayer() {
            Layer rootLayer = mView.getLayerClient().getRoot();
            if (rootLayer == null) {
                return;
            }

            rootLayer.draw(mPageContext);
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapElementForJNI(allowMultithread = true)
        public void drawForeground() {
            /* Draw any extra layers that were added (likely plugins) */
            if (mExtraLayers.size() > 0) {
                for (Layer layer : mExtraLayers) {
                    layer.draw(mPageContext);
                }
            }

            /* Draw the vertical scrollbar. */
            if (mPageRect.height() > mFrameMetrics.getHeight())
                mVertScrollLayer.draw(mPageContext);

            /* Draw the horizontal scrollbar. */
            if (mPageRect.width() > mFrameMetrics.getWidth())
                mHorizScrollLayer.draw(mPageContext);

            /* Measure how much of the screen is checkerboarding */
            Layer rootLayer = mView.getLayerClient().getRoot();
            if ((rootLayer != null) &&
                (mProfileRender || PanningPerfAPI.isRecordingCheckerboard())) {
                // Calculate the incompletely rendered area of the page
                float checkerboard =  1.0f - GeckoAppShell.computeRenderIntegrity();

                PanningPerfAPI.recordCheckerboard(checkerboard);
                if (checkerboard < 0.0f || checkerboard > 1.0f) {
                    Log.e(LOGTAG, "Checkerboard value out of bounds: " + checkerboard);
                }

                mCompleteFramesRendered += 1.0f - checkerboard;
                mFramesRendered ++;

                if (mFrameStartTime - mProfileOutputTime > NANOS_PER_SECOND) {
                    mProfileOutputTime = mFrameStartTime;
                    printCheckerboardStats();
                }
            }

            runRenderTasks(mTasks, true, mFrameStartTime);

            /* Draw the FPS. */
            if (mFrameRateLayer != null) {
                updateDroppedFrames(mFrameStartTime);

                GLES20.glEnable(GLES20.GL_BLEND);
                GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);
                mFrameRateLayer.draw(mScreenContext);
            }
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        @WrapElementForJNI(allowMultithread = true)
        public void endDrawing() {
            // If a layer update requires further work, schedule another redraw
            if (!mUpdated)
                mView.requestRender();

            PanningPerfAPI.recordFrameTime();

            /* Used by robocop for testing purposes */
            IntBuffer pixelBuffer = mPixelBuffer;
            if (mUpdated && pixelBuffer != null) {
                synchronized (pixelBuffer) {
                    pixelBuffer.position(0);
                    GLES20.glReadPixels(0, 0, (int)mScreenContext.viewport.width(),
                                        (int)mScreenContext.viewport.height(), GLES20.GL_RGBA,
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
                        mView.getChildAt(0).setBackgroundColor(Color.TRANSPARENT);
                    }
                });
                mView.setPaintState(LayerView.PAINT_AFTER_FIRST);
            }
            mLastFrameTime = mFrameStartTime;
        }
    }

    @Override
    public void onTabChanged(final Tab tab, Tabs.TabEvents msg, Object data) {
        // Sets the background of the newly selected tab. This background color
        // gets cleared in endDrawing(). This function runs on the UI thread,
        // but other code that touches the paint state is run on the compositor
        // thread, so this may need to be changed if any problems appear.
        if (msg == Tabs.TabEvents.SELECTED) {
            mView.getChildAt(0).setBackgroundColor(tab.getBackgroundColor());
            mView.setPaintState(LayerView.PAINT_START);
        }
    }
}
