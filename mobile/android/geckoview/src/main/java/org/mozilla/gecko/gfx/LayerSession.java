/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.support.annotation.NonNull;
import android.util.Log;
import android.view.Surface;

public class LayerSession {
    private static final String LOGTAG = "GeckoLayerSession";
    private static final boolean DEBUG = false;

    //
    // NOTE: These values are also defined in
    // gfx/layers/ipc/UiCompositorControllerMessageTypes.h and must be kept in sync. Any
    // new AnimatorMessageType added here must also be added there.
    //
    // Sent from compositor when the static toolbar wants to hide.
    /* package */ final static int STATIC_TOOLBAR_NEEDS_UPDATE      = 0;
    // Sent from compositor when the static toolbar image has been updated and is ready to
    // animate.
    /* package */ final static int STATIC_TOOLBAR_READY             = 1;
    // Sent to compositor when the real toolbar has been hidden.
    /* package */ final static int TOOLBAR_HIDDEN                   = 2;
    // Sent to compositor when the real toolbar is visible.
    /* package */ final static int TOOLBAR_VISIBLE                  = 3;
    // Sent from compositor when the static toolbar has been made visible so the real
    // toolbar should be shown.
    /* package */ final static int TOOLBAR_SHOW                     = 4;
    // Sent from compositor after first paint
    /* package */ final static int FIRST_PAINT                      = 5;
    // Sent to compositor requesting toolbar be shown immediately
    /* package */ final static int REQUEST_SHOW_TOOLBAR_IMMEDIATELY = 6;
    // Sent to compositor requesting toolbar be shown animated
    /* package */ final static int REQUEST_SHOW_TOOLBAR_ANIMATED    = 7;
    // Sent to compositor requesting toolbar be hidden immediately
    /* package */ final static int REQUEST_HIDE_TOOLBAR_IMMEDIATELY = 8;
    // Sent to compositor requesting toolbar be hidden animated
    /* package */ final static int REQUEST_HIDE_TOOLBAR_ANIMATED    = 9;
    // Sent from compositor when a layer has been updated
    /* package */ final static int LAYERS_UPDATED                   = 10;
    // Sent to compositor when the toolbar snapshot fails.
    /* package */ final static int TOOLBAR_SNAPSHOT_FAILED          = 11;
    // Special message sent from UiCompositorControllerChild once it is open
    /* package */ final static int COMPOSITOR_CONTROLLER_OPEN       = 20;
    // Special message sent from controller to query if the compositor controller is open.
    /* package */ final static int IS_COMPOSITOR_CONTROLLER_OPEN    = 21;

    protected class Compositor extends JNIObject {
        public boolean isReady() {
            return LayerSession.this.isCompositorReady();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorAttached() {
            LayerSession.this.onCompositorAttached();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorDetached() {
            // Clear out any pending calls on the UI thread.
            LayerSession.this.onCompositorDetached();
            disposeNative();
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        @Override protected native void disposeNative();

        @WrapForJNI(calledFrom = "any", dispatchTo = "gecko")
        public native void attachNPZC(NativePanZoomController npzc);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        public native void onBoundsChanged(int left, int top, int width, int height);

        // Gecko thread creates compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "proxy")
        public native void createCompositor(int width, int height, Object surface);

        // Gecko thread pauses compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncPauseCompositor();

        // UI thread resumes compositor and notifies Gecko thread; does not block UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void syncResumeResizeCompositor(int width, int height, Object surface);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setMaxToolbarHeight(int height);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setPinned(boolean pinned, int reason);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void sendToolbarAnimatorMessage(int message);

        @WrapForJNI(calledFrom = "ui")
        private void recvToolbarAnimatorMessage(int message) {
            LayerSession.this.handleCompositorMessage(message);
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void setDefaultClearColor(int color);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void requestScreenPixels();

        @WrapForJNI(calledFrom = "ui")
        private void recvScreenPixels(int width, int height, int[] pixels) {
            LayerSession.this.recvScreenPixels(width, height, pixels);
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void enableLayerUpdateNotifications(boolean enable);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        public native void sendToolbarPixelsToCompositor(final int width, final int height,
                                                         final int[] pixels);

        // The compositor invokes this function just before compositing a frame where the
        // document is different from the document composited on the last frame. In these
        // cases, the viewport information we have in Java is no longer valid and needs to
        // be replaced with the new viewport information provided.
        @WrapForJNI(calledFrom = "ui")
        private void updateRootFrameMetrics(float scrollX, float scrollY, float zoom) {
            LayerSession.this.onMetricsChanged(scrollX, scrollY, zoom);
        }

        @WrapForJNI(calledFrom = "ui")
        private void updateOverscrollVelocity(final float x, final float y) {
            LayerSession.this.updateOverscrollVelocity(x, y);
        }

        @WrapForJNI(calledFrom = "ui")
        private void updateOverscrollOffset(final float x, final float y) {
            LayerSession.this.updateOverscrollOffset(x, y);
        }

        @WrapForJNI(calledFrom = "ui")
        private void onSelectionCaretDrag(final boolean dragging) {
            // Active SelectionCaretDrag requires DynamicToolbarAnimator to be pinned to
            // avoid unwanted scroll interactions.
            LayerSession.this.onSelectionCaretDrag(dragging);
        }
    }

    protected final Compositor mCompositor = new Compositor();

    // All fields are accessed on UI thread only.
    private final GeckoDisplay mDisplay = new GeckoDisplay(this);
    private NativePanZoomController mNPZC;
    private OverscrollEdgeEffect mOverscroll;
    private DynamicToolbarAnimator mToolbar;
    private CompositorController mController;

    private boolean mAttachedCompositor;
    private boolean mCalledCreateCompositor;
    private boolean mCompositorReady;
    private Surface mSurface;

    // All fields of coordinates are in screen units.
    private int mLeft;
    private int mTop; // Top of the surface (including toolbar);
    private int mClientTop; // Top of the client area (i.e. excluding toolbar);
    private int mWidth;
    private int mHeight; // Height of the surface (including toolbar);
    private int mClientHeight; // Height of the client area (i.e. excluding toolbar);
    private float mViewportLeft;
    private float mViewportTop;
    private float mViewportZoom = 1.0f;

    /* package */ GeckoDisplay getDisplay() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }
        return mDisplay;
    }

    /**
     * Get the NativePanZoomController instance for this session.
     *
     * @return NativePanZoomController instance.
     */
    public NativePanZoomController getPanZoomController() {
        ThreadUtils.assertOnUiThread();

        if (mNPZC == null) {
            mNPZC = new NativePanZoomController(this);
            if (mAttachedCompositor) {
                mCompositor.attachNPZC(mNPZC);
            }
        }
        return mNPZC;
    }

    /**
     * Get the OverscrollEdgeEffect instance for this session.
     *
     * @return OverscrollEdgeEffect instance.
     */
    public OverscrollEdgeEffect getOverscrollEdgeEffect() {
        ThreadUtils.assertOnUiThread();

        if (mOverscroll == null) {
            mOverscroll = new OverscrollEdgeEffect(this);
        }
        return mOverscroll;
    }

    /**
     * Get the DynamicToolbarAnimator instance for this session.
     *
     * @return DynamicToolbarAnimator instance.
     */
    public @NonNull DynamicToolbarAnimator getDynamicToolbarAnimator() {
        ThreadUtils.assertOnUiThread();

        if (mToolbar == null) {
            mToolbar = new DynamicToolbarAnimator(this);
        }
        return mToolbar;
    }

    /**
     * Get the CompositorController instance for this session.
     *
     * @return CompositorController instance.
     */
    public @NonNull CompositorController getCompositorController() {
        ThreadUtils.assertOnUiThread();

        if (mController == null) {
            mController = new CompositorController(this);
            if (mCompositorReady) {
                mController.onCompositorReady();
            }
        }
        return mController;
    }

    /**
     * Get a matrix for transforming from client coordinates to screen coordinates. The
     * client coordinates are in CSS pixels and are relative to the viewport origin; their
     * relation to screen coordinates does not depend on the current scroll position.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getClientToSurfaceMatrix(Matrix)
     * @see #getPageToScreenMatrix(Matrix)
     */
    public void getClientToScreenMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getClientToSurfaceMatrix(matrix);
        matrix.postTranslate(mLeft, mTop);
    }

    /**
     * Get a matrix for transforming from client coordinates to surface coordinates.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getClientToScreenMatrix(Matrix)
     * @see #getPageToSurfaceMatrix(Matrix)
     */
    public void getClientToSurfaceMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        matrix.setScale(mViewportZoom, mViewportZoom);
        if (mClientTop != mTop) {
            matrix.postTranslate(0, mClientTop - mTop);
        }
    }

    /**
     * Get a matrix for transforming from page coordinates to screen coordinates. The page
     * coordinates are in CSS pixels and are relative to the page origin; their relation
     * to screen coordinates depends on the current scroll position of the outermost
     * frame.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getPageToSurfaceMatrix(Matrix)
     * @see #getClientToScreenMatrix(Matrix)
     */
    public void getPageToScreenMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getPageToSurfaceMatrix(matrix);
        matrix.postTranslate(mLeft, mTop);
    }

    /**
     * Get a matrix for transforming from page coordinates to surface coordinates.
     *
     * @param matrix Matrix to be replaced by the transformation matrix.
     * @see #getPageToScreenMatrix(Matrix)
     * @see #getClientToSurfaceMatrix(Matrix)
     */
    public void getPageToSurfaceMatrix(@NonNull final Matrix matrix) {
        ThreadUtils.assertOnUiThread();

        getClientToSurfaceMatrix(matrix);
        matrix.postTranslate(-mViewportLeft, -mViewportTop);
    }

    /**
     * Get the bounds of the client area in client coordinates. The returned top-left
     * coordinates are always (0, 0). Use the matrix from {@link
     * #getClientToSurfaceMatrix(Matrix)} or {@link #getClientToScreenMatrix(Matrix)} to
     * map these bounds to surface or screen coordinates, respectively.
     *
     * @param rect RectF to be replaced by the client bounds in client coordinates.
     * @see getSurfaceBounds(Rect)
     */
    public void getClientBounds(@NonNull final RectF rect) {
        ThreadUtils.assertOnUiThread();

        rect.set(0.0f, 0.0f, (float) mWidth / mViewportZoom,
                             (float) mClientHeight / mViewportZoom);
    }

    /**
     * Get the bounds of the client area in surface coordinates. This is equivalent to
     * mapping the bounds returned by #getClientBounds(RectF) with the matrix returned by
     * #getClientToSurfaceMatrix(Matrix).
     *
     * @param rect Rect to be replaced by the client bounds in surface coordinates.
     */
    public void getSurfaceBounds(@NonNull final Rect rect) {
        ThreadUtils.assertOnUiThread();

        rect.set(0, mClientTop - mTop, mWidth, mHeight);
    }

    @WrapForJNI(stubName = "GetCompositor", calledFrom = "ui")
    private Object getCompositorFromNative() {
        // Only used by native code.
        return mCompositorReady ? mCompositor : null;
    }

    /* package */ void onCompositorAttached() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mAttachedCompositor = true;

        if (mNPZC != null) {
            mCompositor.attachNPZC(mNPZC);
        }

        if (mSurface != null) {
            // If we have a valid surface, create the compositor now that we're attached.
            // Leave mSurface alone because we'll need it later for onCompositorReady.
            onSurfaceChanged(mSurface, mWidth, mHeight);
        }
    }

    /* package */ void onCompositorDetached() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mController != null) {
            mController.onCompositorDetached();
        }

        mAttachedCompositor = false;
        mCalledCreateCompositor = false;
        mCompositorReady = false;
    }

    /* package */ void handleCompositorMessage(final int message) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        switch (message) {
            case COMPOSITOR_CONTROLLER_OPEN: {
                if (isCompositorReady()) {
                    return;
                }

                // Delay calling onCompositorReady to avoid deadlock due
                // to synchronous call to the compositor.
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        onCompositorReady();
                    }
                });
                break;
            }

            case FIRST_PAINT: {
                if (mController != null) {
                    mController.onFirstPaint();
                }
                break;
            }

            case LAYERS_UPDATED: {
                if (mController != null) {
                    mController.notifyDrawCallbacks();
                }
                break;
            }

            case STATIC_TOOLBAR_READY:
            case TOOLBAR_SHOW: {
                if (mToolbar != null) {
                    mToolbar.handleToolbarAnimatorMessage(message);
                    // Update window bounds due to toolbar visibility change.
                    onWindowBoundsChanged();
                }
                break;
            }

            default: {
                if (mToolbar != null) {
                    mToolbar.handleToolbarAnimatorMessage(message);
                } else {
                    Log.w(LOGTAG, "Unexpected message: " + message);
                }
                break;
            }
        }
    }

    /* package */ void recvScreenPixels(int width, int height, int[] pixels) {
        if (mController != null) {
            mController.recvScreenPixels(width, height, pixels);
        }
    }

    /* package */ boolean isCompositorReady() {
        return mCompositorReady;
    }

    /* package */ void onCompositorReady() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mCompositorReady = true;

        if (mController != null) {
            mController.onCompositorReady();
        }

        if (mSurface != null) {
            // If we have a valid surface, resume the
            // compositor now that the compositor is ready.
            onSurfaceChanged(mSurface, mWidth, mHeight);
            mSurface = null;
        }

        if (mToolbar != null) {
            mToolbar.onCompositorReady();
        }
    }

    /* package */ void updateOverscrollVelocity(final float x, final float y) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mOverscroll == null) {
            return;
        }

        // Multiply the velocity by 1000 to match what was done in JPZ.
        mOverscroll.setVelocity(x * 1000.0f, OverscrollEdgeEffect.AXIS_X);
        mOverscroll.setVelocity(y * 1000.0f, OverscrollEdgeEffect.AXIS_Y);
    }

    /* package */ void updateOverscrollOffset(final float x, final float y) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mOverscroll == null) {
            return;
        }

        mOverscroll.setDistance(x, OverscrollEdgeEffect.AXIS_X);
        mOverscroll.setDistance(y, OverscrollEdgeEffect.AXIS_Y);
    }

    /* package */ void onSelectionCaretDrag(final boolean dragging) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mToolbar != null) {
            mToolbar.setPinned(dragging, DynamicToolbarAnimator.PinReason.CARET_DRAG);
        }
    }

    /* package */ void onMetricsChanged(final float scrollX, final float scrollY,
                                        final float zoom) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mViewportLeft = scrollX;
        mViewportTop = scrollY;
        mViewportZoom = zoom;
    }

    /* protected */ void onWindowBoundsChanged() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        final int toolbarHeight;
        if (mToolbar != null) {
            toolbarHeight = mToolbar.getCurrentToolbarHeight();
        } else {
            toolbarHeight = 0;
        }

        mClientTop = mTop + toolbarHeight;
        mClientHeight = mHeight - toolbarHeight;

        if (mAttachedCompositor) {
            mCompositor.onBoundsChanged(mLeft, mClientTop, mWidth, mClientHeight);
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(mWidth, mClientHeight);
        }
    }

    /* package */ void onSurfaceChanged(final Surface surface, final int width,
                                        final int height) {
        ThreadUtils.assertOnUiThread();

        mWidth = width;
        mHeight = height;

        if (mCompositorReady) {
            mCompositor.syncResumeResizeCompositor(width, height, surface);
            onWindowBoundsChanged();
            return;
        }

        if (mAttachedCompositor && !mCalledCreateCompositor) {
            mCompositor.createCompositor(width, height, surface);
            mCompositor.sendToolbarAnimatorMessage(IS_COMPOSITOR_CONTROLLER_OPEN);
            mCalledCreateCompositor = true;
        }

        // We have a valid surface but we're not attached or the compositor
        // is not ready; save the surface for later when we're ready.
        mSurface = surface;

        // Adjust bounds as the last step.
        onWindowBoundsChanged();
    }

    /* package */ void onSurfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        if (mCompositorReady) {
            mCompositor.syncPauseCompositor();
            return;
        }

        // While the surface was valid, we never became attached or the
        // compositor never became ready; clear the saved surface.
        mSurface = null;
    }

    /* package */ void onScreenOriginChanged(final int left, final int top) {
        ThreadUtils.assertOnUiThread();

        if (mLeft == left && mTop == top) {
            return;
        }

        mLeft = left;
        mTop = top;
        onWindowBoundsChanged();
    }

    /**
     * Acquire the GeckoDisplay instance for providing the session with a drawing Surface.
     * Be sure to call {@link GeckoDisplay#surfaceChanged(Surface, int, int)} on the
     * acquired display if there is already a valid Surface.
     *
     * @return GeckoDisplay instance.
     * @see #releaseDisplay(GeckoDisplay)
     */
    public @NonNull GeckoDisplay acquireDisplay() {
        ThreadUtils.assertOnUiThread();

        return mDisplay;
    }

    /**
     * Release an acquired GeckoDisplay instance. Be sure to call {@link
     * GeckoDisplay#surfaceDestroyed()} before releasing the display if it still has a
     * valid Surface.
     *
     * @param display Acquired GeckoDisplay instance.
     * @see #acquireDisplay()
     */
    public void releaseDisplay(final @NonNull GeckoDisplay display) {
        ThreadUtils.assertOnUiThread();

        if (display != mDisplay) {
            throw new IllegalArgumentException("Display not attached");
        }
    }
}
