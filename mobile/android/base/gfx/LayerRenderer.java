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
 *   Arkady Blyakher <rkadyb@mit.edu>
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
import org.mozilla.gecko.gfx.NinePatchTileLayer;
import org.mozilla.gecko.gfx.SingleTileLayer;
import org.mozilla.gecko.gfx.TextureReaper;
import org.mozilla.gecko.gfx.TextureGenerator;
import org.mozilla.gecko.gfx.TextLayer;
import org.mozilla.gecko.gfx.TileLayer;
import org.mozilla.gecko.GeckoAppShell;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.ArrayList;

/**
 * The layer renderer implements the rendering logic for a layer view.
 */
public class LayerRenderer implements GLSurfaceView.Renderer {
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
    private final CheckerboardImage mCheckerboardImage;
    private final SingleTileLayer mCheckerboardLayer;
    private final NinePatchTileLayer mShadowLayer;
    private TextLayer mFrameRateLayer;
    private final ScrollbarLayer mHorizScrollLayer;
    private final ScrollbarLayer mVertScrollLayer;
    private final FadeRunnable mFadeRunnable;
    private final FloatBuffer mCoordBuffer;
    private RenderContext mLastPageContext;
    private int mMaxTextureSize;

    private ArrayList<Layer> mExtraLayers = new ArrayList<Layer>();

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
    public static final String DEFAULT_VERTEX_SHADER =
        "uniform mat4 uTMatrix;\n" +
        "attribute vec4 vPosition;\n" +
        "attribute vec2 aTexCoord;\n" +
        "varying vec2 vTexCoord;\n" +
        "void main() {\n" +
        "    gl_Position = uTMatrix * vPosition;\n" +
        "    vTexCoord = aTexCoord;\n" +
        "}\n";

    // Note we flip the y-coordinate in the fragment shader from a
    // coordinate system with (0,0) in the top left to one with (0,0) in
    // the bottom left.
    public static final String DEFAULT_FRAGMENT_SHADER =
        "precision mediump float;\n" +
        "varying vec2 vTexCoord;\n" +
        "uniform sampler2D sTexture;\n" +
        "void main() {\n" +
        "    gl_FragColor = texture2D(sTexture, vec2(vTexCoord.x, 1.0 - vTexCoord.y));\n" +
        "}\n";

    public LayerRenderer(LayerView view) {
        mView = view;

        LayerController controller = view.getController();

        CairoImage backgroundImage = new BufferedCairoImage(controller.getBackgroundPattern());
        mBackgroundLayer = new SingleTileLayer(true, backgroundImage);

        mCheckerboardImage = new CheckerboardImage();
        mCheckerboardLayer = new SingleTileLayer(true, mCheckerboardImage);

        CairoImage shadowImage = new BufferedCairoImage(controller.getShadowPattern());
        mShadowLayer = new NinePatchTileLayer(shadowImage);

        mHorizScrollLayer = ScrollbarLayer.create(this, false);
        mVertScrollLayer = ScrollbarLayer.create(this, true);
        mFadeRunnable = new FadeRunnable();

        mFrameTimings = new int[60];
        mCurrentFrame = mFrameTimingsSum = mDroppedFrames = 0;

        // Initialize the FloatBuffer that will be used to store all vertices and texture
        // coordinates in draw() commands.
        ByteBuffer byteBuffer = GeckoAppShell.allocateDirectBuffer(COORD_BUFFER_SIZE * 4);
        byteBuffer.order(ByteOrder.nativeOrder());
        mCoordBuffer = byteBuffer.asFloatBuffer();
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
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

        TextureGenerator.get().fill();

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
        LayerController controller = mView.getController();

        synchronized (controller) {
            if (mExtraLayers.contains(layer)) {
                mExtraLayers.remove(layer);
            }

            mExtraLayers.add(layer);
        }
    }

    public void removeLayer(Layer layer) {
        LayerController controller = mView.getController();

        synchronized (controller) {
            mExtraLayers.remove(layer);
        }
    }

    /**
     * Called whenever a new frame is about to be drawn.
     */
    public void onDrawFrame(GL10 gl) {
        RenderContext pageContext = createPageContext(), screenContext = createScreenContext();
        Frame frame = createFrame(pageContext, screenContext);
        synchronized (mView.getController()) {
            frame.beginDrawing();
            frame.drawBackground();
            frame.drawRootLayer();
            frame.drawForeground();
            frame.endDrawing();
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

    public RenderContext createScreenContext() {
        LayerController layerController = mView.getController();
        IntSize viewportSize = new IntSize(layerController.getViewportSize());
        RectF viewport = new RectF(0.0f, 0.0f, viewportSize.width, viewportSize.height);
        FloatSize pageSize = new FloatSize(layerController.getPageSize());
        return createContext(viewport, pageSize, 1.0f);
    }

    public RenderContext createPageContext() {
        LayerController layerController = mView.getController();

        Rect viewport = new Rect();
        layerController.getViewport().round(viewport);

        FloatSize pageSize = new FloatSize(layerController.getPageSize());
        float zoomFactor = layerController.getZoomFactor();
        return createContext(new RectF(viewport), pageSize, zoomFactor);
    }

    private RenderContext createContext(RectF viewport, FloatSize pageSize, float zoomFactor) {
        return new RenderContext(viewport, pageSize, zoomFactor, mPositionHandle, mTextureHandle,
                                 mCoordBuffer);
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
        GLES20.glViewport(0, 0, width, height);

        if (mFrameRateLayer != null) {
            moveFrameRateLayer(width, height);
        }

        // updating the state in the view/controller/client should be
        // done on the main UI thread, not the GL renderer thread
        mView.post(new Runnable() {
            public void run() {
                mView.setViewportSize(new IntSize(width, height));
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

    private void updateCheckerboardLayer(RenderContext renderContext) {
        int checkerboardColor = mView.getController().getCheckerboardColor();
        boolean showChecks = mView.getController().checkerboardShouldShowChecks();
        if (checkerboardColor == mCheckerboardImage.getColor() &&
            showChecks == mCheckerboardImage.getShowChecks()) {
            return;
        }

        mCheckerboardLayer.beginTransaction();  // called on compositor thread
        try {
            mCheckerboardImage.update(showChecks, checkerboardColor);
            mCheckerboardLayer.invalidate();
        } finally {
            mCheckerboardLayer.endTransaction();
        }

        mCheckerboardLayer.update(renderContext);   // called on compositor thread
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

    public Frame createFrame(RenderContext pageContext, RenderContext screenContext) {
        return new Frame(pageContext, screenContext);
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
        // A rendering context for page-positioned layers, and one for screen-positioned layers.
        private RenderContext mPageContext, mScreenContext;
        // Whether a layer was updated.
        private boolean mUpdated;

        public Frame(RenderContext pageContext, RenderContext screenContext) {
            mPageContext = pageContext;
            mScreenContext = screenContext;
        }

        private void setScissorRect() {
            Rect scissorRect = transformToScissorRect(getPageRect());
            GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
            GLES20.glScissor(scissorRect.left, scissorRect.top,
                             scissorRect.width(), scissorRect.height());
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        public void beginDrawing() {
            mFrameStartTime = SystemClock.uptimeMillis();

            TextureReaper.get().reap();
            TextureGenerator.get().fill();

            mUpdated = true;

            LayerController controller = mView.getController();
            Layer rootLayer = controller.getRoot();

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
            updateCheckerboardLayer(mScreenContext);
            if (mFrameRateLayer != null) mUpdated &= mFrameRateLayer.update(mScreenContext); // called on compositor thread
            mUpdated &= mVertScrollLayer.update(mPageContext);  // called on compositor thread
            mUpdated &= mHorizScrollLayer.update(mPageContext); // called on compositor thread

            for (Layer layer : mExtraLayers)
                mUpdated &= layer.update(mPageContext); // called on compositor thread

            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);

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
        }

        /** This function is invoked via JNI; be careful when modifying signature. */
        public void drawBackground() {
            /* Draw the background. */
            mBackgroundLayer.setMask(getPageRect());
            mBackgroundLayer.draw(mScreenContext);

            /* Draw the drop shadow, if we need to. */
            Rect pageRect = getPageRect();
            RectF untransformedPageRect = new RectF(0.0f, 0.0f, pageRect.width(),
                                                    pageRect.height());
            if (!untransformedPageRect.contains(mView.getController().getViewport()))
                mShadowLayer.draw(mPageContext);

            /* Find the area the root layer will render into, to mask the scissor rect */
            Rect rootMask = null;
            Layer rootLayer = mView.getController().getRoot();
            if (rootLayer != null) {
                RectF rootBounds = rootLayer.getBounds(mPageContext);
                rootBounds.offset(-mPageContext.viewport.left, -mPageContext.viewport.top);
                rootMask = new Rect();
                rootBounds.roundOut(rootMask);
            }

            /* Draw the checkerboard. */
            setScissorRect();
            mCheckerboardLayer.setMask(rootMask);
            mCheckerboardLayer.draw(mScreenContext);
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
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
            Rect pageRect = getPageRect();
            LayerController controller = mView.getController();

            /* Draw any extra layers that were added (likely plugins) */
            for (Layer layer : mExtraLayers)
                layer.draw(mPageContext);

            /* Draw the vertical scrollbar. */
            IntSize screenSize = new IntSize(controller.getViewportSize());
            if (pageRect.height() > screenSize.height)
                mVertScrollLayer.draw(mPageContext);

            /* Draw the horizontal scrollbar. */
            if (pageRect.width() > screenSize.width)
                mHorizScrollLayer.draw(mPageContext);

            /* Measure how much of the screen is checkerboarding */
            Layer rootLayer = controller.getRoot();
            if ((rootLayer != null) &&
                (mProfileRender || PanningPerfAPI.isRecordingCheckerboard())) {
                // Find out how much of the viewport area is valid
                Rect viewport = RectUtils.round(mPageContext.viewport);
                Region validRegion = rootLayer.getValidRegion(mPageContext);
                validRegion.op(viewport, Region.Op.INTERSECT);

                float checkerboard = 0.0f;
                if (!(validRegion.isRect() && validRegion.getBounds().equals(viewport))) {
                    int screenArea = viewport.width() * viewport.height();
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
        }
    }
}
