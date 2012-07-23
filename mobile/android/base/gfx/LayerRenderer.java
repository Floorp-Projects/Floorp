/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.gfx.Layer.RenderContext;
import org.mozilla.gecko.GeckoAppShell;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.opengl.GLES20;
import android.os.SystemClock;
import android.util.Log;

import javax.microedition.khronos.egl.EGLConfig;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer {
    private static final String LOGTAG = "GeckoLayerRenderer";
    private static final String PROFTAG = "GeckoLayerRendererProf";

    /*
     * The amount of time a frame is allowed to take to render before we declare it a dropped
     * frame.
     */
    private static final int MAX_FRAME_TIME = 16;   /* 1000 ms / 60 FPS */

    private static final int FRAME_RATE_METER_WIDTH = 128;
    private static final int FRAME_RATE_METER_HEIGHT = 32;

    private final LayerView mView;
    private final SingleTileLayer mBackgroundLayer;
    private final ScreenshotLayer mCheckerboardLayer;
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

    public void setCheckerboardBitmap(ByteBuffer data, int width, int height, RectF pageRect, Rect copyRect) {
        try {
            mCheckerboardLayer.setBitmap(data, width, height, copyRect);
        } catch (IllegalArgumentException ex) {
            Log.e(LOGTAG, "error setting bitmap: ", ex);
        }
        mCheckerboardLayer.beginTransaction();
        try {
            mCheckerboardLayer.setPosition(RectUtils.round(pageRect));
            mCheckerboardLayer.invalidate();
        } finally {
            mCheckerboardLayer.endTransaction();
        }
    }

    public void resetCheckerboard() {
        mCheckerboardLayer.reset();
    }

    public LayerRenderer(LayerView view) {
        mView = view;

        LayerController controller = view.getController();

        CairoImage backgroundImage = new BufferedCairoImage(controller.getBackgroundPattern());
        mBackgroundLayer = new SingleTileLayer(true, backgroundImage);

        mCheckerboardLayer = ScreenshotLayer.create();

        CairoImage shadowImage = new BufferedCairoImage(controller.getShadowPattern());
        mShadowLayer = new NinePatchTileLayer(shadowImage);

        mHorizScrollLayer = ScrollbarLayer.create(this, false);
        mVertScrollLayer = ScrollbarLayer.create(this, true);
        mFadeRunnable = new FadeRunnable();

        mFrameTimings = new int[60];
        mCurrentFrame = mFrameTimingsSum = mDroppedFrames = 0;

        // Initialize the FloatBuffer that will be used to store all vertices and texture
        // coordinates in draw() commands.
        mCoordByteBuffer = GeckoAppShell.allocateDirectBuffer(COORD_BUFFER_SIZE * 4);
        mCoordByteBuffer.order(ByteOrder.nativeOrder());
        mCoordBuffer = mCoordByteBuffer.asFloatBuffer();
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            if (mCoordByteBuffer != null) {
                GeckoAppShell.freeDirectBuffer(mCoordByteBuffer);
                mCoordByteBuffer = null;
                mCoordBuffer = null;
            }
        } finally {
            super.finalize();
        }
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

    private RenderContext createScreenContext(ImmutableViewportMetrics metrics) {
        RectF viewport = new RectF(0.0f, 0.0f, metrics.getWidth(), metrics.getHeight());
        RectF pageRect = new RectF(metrics.getPageRect());
        return createContext(viewport, pageRect, 1.0f);
    }

    private RenderContext createPageContext(ImmutableViewportMetrics metrics) {
        Rect viewport = RectUtils.round(metrics.getViewport());
        RectF pageRect = metrics.getPageRect();
        float zoomFactor = metrics.zoomFactor;
        return createContext(new RectF(viewport), pageRect, zoomFactor);
    }

    private RenderContext createContext(RectF viewport, RectF pageRect, float zoomFactor) {
        return new RenderContext(viewport, pageRect, zoomFactor, mPositionHandle, mTextureHandle,
                                 mCoordBuffer);
    }

    private void updateDroppedFrames(long frameStartTime) {
        int frameElapsedTime = (int)(SystemClock.uptimeMillis() - frameStartTime);

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

        public Frame(ImmutableViewportMetrics metrics) {
            mFrameMetrics = metrics;
            mPageContext = createPageContext(metrics);
            mScreenContext = createScreenContext(metrics);
            mPageRect = getPageRect();
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

            return new Rect(left, screenSize.height - bottom, right,
                            (screenSize.height - bottom) + (bottom - top));
        }

        private Rect getPageRect() {
            Point origin = PointUtils.round(mFrameMetrics.getOrigin());
            Rect pageRect = RectUtils.round(mFrameMetrics.getPageRect());
            pageRect.offset(-origin.x, -origin.y);
            return pageRect;
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        public void beginDrawing() {
            mFrameStartTime = SystemClock.uptimeMillis();

            TextureReaper.get().reap();
            TextureGenerator.get().fill();

            mUpdated = true;

            Layer rootLayer = mView.getController().getRoot();

            if (!mPageContext.fuzzyEquals(mLastPageContext)) {
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
            mLastPageContext = mPageContext;

            /* Update layers. */
            if (rootLayer != null) mUpdated &= rootLayer.update(mPageContext);  // called on compositor thread
            mUpdated &= mBackgroundLayer.update(mScreenContext);    // called on compositor thread
            mUpdated &= mShadowLayer.update(mPageContext);  // called on compositor thread
            mUpdated &= mCheckerboardLayer.update(mPageContext);   // called on compositor thread
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

        /** This function is invoked via JNI; be careful when modifying signature. */
        public void drawBackground() {
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);

            /* Update background color. */
            mBackgroundColor = mView.getController().getCheckerboardColor();

            /* Clear to the page background colour. The bits set here need to
             * match up with those used in gfx/layers/opengl/LayerManagerOGL.cpp.
             */
            GLES20.glClearColor(((mBackgroundColor>>16)&0xFF) / 255.0f,
                                ((mBackgroundColor>>8)&0xFF) / 255.0f,
                                (mBackgroundColor&0xFF) / 255.0f,
                                0.0f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT |
                           GLES20.GL_DEPTH_BUFFER_BIT);

            /* Draw the background. */
            mBackgroundLayer.setMask(mPageRect);
            mBackgroundLayer.draw(mScreenContext);

            /* Draw the drop shadow, if we need to. */
            RectF untransformedPageRect = new RectF(0.0f, 0.0f, mPageRect.width(),
                                                    mPageRect.height());
            if (!untransformedPageRect.contains(mView.getController().getViewport()))
                mShadowLayer.draw(mPageContext);

            /* Draw the 'checkerboard'. We use gfx.show_checkerboard_pattern to
             * determine whether to draw the screenshot layer.
             */
            if (mView.getController().checkerboardShouldShowChecks()) {
                /* Find the area the root layer will render into, to mask the checkerboard layer */
                Rect rootMask = getMaskForLayer(mView.getController().getRoot());
                mCheckerboardLayer.setMask(rootMask);

                /* Scissor around the page-rect, in case the page has shrunk
                 * since the screenshot layer was last updated.
                 */
                setScissorRect(); // Calls glEnable(GL_SCISSOR_TEST))
                mCheckerboardLayer.draw(mPageContext);
            }
        }

        // Draws the layer the client added to us.
        void drawRootLayer() {
            Layer rootLayer = mView.getController().getRoot();
            if (rootLayer == null) {
                return;
            }

            rootLayer.draw(mPageContext);
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        public void drawForeground() {
            /* Draw any extra layers that were added (likely plugins) */
            if (mExtraLayers.size() > 0) {
                for (Layer layer : mExtraLayers) {
                    if (!layer.usesDefaultProgram())
                        deactivateDefaultProgram();

                    layer.draw(mPageContext);

                    if (!layer.usesDefaultProgram())
                        activateDefaultProgram();
                }
            }

            /* Draw the vertical scrollbar. */
            if (mPageRect.height() > mFrameMetrics.getHeight())
                mVertScrollLayer.draw(mPageContext);

            /* Draw the horizontal scrollbar. */
            if (mPageRect.width() > mFrameMetrics.getWidth())
                mHorizScrollLayer.draw(mPageContext);

            /* Measure how much of the screen is checkerboarding */
            Layer rootLayer = mView.getController().getRoot();
            if ((rootLayer != null) &&
                (mProfileRender || PanningPerfAPI.isRecordingCheckerboard())) {
                // Find out how much of the viewport area is valid
                Rect viewport = RectUtils.round(mPageContext.viewport);
                Region validRegion = rootLayer.getValidRegion(mPageContext);

                /* restrict the viewport to page bounds so we don't
                 * count overscroll as checkerboard */
                if (!viewport.intersect(mPageRect)) {
                    /* if the rectangles don't intersect
                       intersect() doesn't change viewport
                       so we set it to empty by hand */
                    viewport.setEmpty();
                }
                validRegion.op(viewport, Region.Op.INTERSECT);

                float checkerboard = 0.0f;

                int screenArea = viewport.width() * viewport.height();
                if (screenArea > 0 && !(validRegion.isRect() && validRegion.getBounds().equals(viewport))) {
                    validRegion.op(viewport, Region.Op.REVERSE_DIFFERENCE);

                    // XXX The assumption here is that a Region never has overlapping
                    //     rects. This is true, as evidenced by reading the SkRegion
                    //     source, but is not mentioned in the Android documentation,
                    //     and so is liable to change.
                    //     If it does change, this code will need to be reevaluated.
                    Rect r = new Rect();
                    int checkerboardArea = 0;
                    for (RegionIterator i = new RegionIterator(validRegion); i.next(r);) {
                        checkerboardArea += r.width() * r.height();
                    }

                    checkerboard = checkerboardArea / (float)screenArea;
                }

                PanningPerfAPI.recordCheckerboard(checkerboard);

                mCompleteFramesRendered += 1.0f - checkerboard;
                mFramesRendered ++;

                if (mFrameStartTime - mProfileOutputTime > 1000) {
                    mProfileOutputTime = mFrameStartTime;
                    printCheckerboardStats();
                }
            }

            /* Draw the FPS. */
            if (mFrameRateLayer != null) {
                updateDroppedFrames(mFrameStartTime);

                GLES20.glEnable(GLES20.GL_BLEND);
                GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);
                mFrameRateLayer.draw(mScreenContext);
            }
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
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

            // Remove white screen once we've painted
            if (mView.getPaintState() == LayerView.PAINT_BEFORE_FIRST) {
                GeckoAppShell.getMainHandler().postAtFrontOfQueue(new Runnable() {
                    public void run() {
                        mView.setBackgroundColor(android.graphics.Color.TRANSPARENT);
                    }
                });
                mView.setPaintState(LayerView.PAINT_AFTER_FIRST);
            }
        }
    }
}
