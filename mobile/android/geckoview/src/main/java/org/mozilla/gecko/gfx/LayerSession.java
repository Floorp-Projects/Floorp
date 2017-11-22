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
        public LayerView layerView;

        public boolean isReady() {
            return LayerSession.this.isCompositorReady();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorAttached() {
            LayerSession.this.onCompositorAttached();
        }

        @WrapForJNI(calledFrom = "ui")
        private void onCompositorDetached() {
            if (layerView != null) {
                layerView.clearDrawListeners();
                layerView = null;
            }
            // Clear out any pending calls on the UI thread.
            LayerSession.this.onCompositorDetached();
            disposeNative();
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        @Override protected native void disposeNative();

        @WrapForJNI(calledFrom = "any", dispatchTo = "gecko")
        public native void attachToJava(NativePanZoomController npzc);

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
            if (layerView != null) {
                layerView.recvScreenPixels(width, height, pixels);
            }
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
    }

    protected final Compositor mCompositor = new Compositor();

    // Following fields are accessed on UI thread.
    private GeckoDisplay mDisplay;
    private DynamicToolbarAnimator mToolbar;
    private ImmutableViewportMetrics mViewportMetrics = new ImmutableViewportMetrics();
    private boolean mAttachedCompositor;
    private boolean mCalledCreateCompositor;
    private boolean mCompositorReady;
    private Surface mSurface;
    private int mLeft;
    private int mTop;
    private int mWidth;
    private int mHeight;

    /* package */ GeckoDisplay getDisplay() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }
        return mDisplay;
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

    public ImmutableViewportMetrics getViewportMetrics() {
        ThreadUtils.assertOnUiThread();
        return mViewportMetrics;
    }

    /* package */ void onCompositorAttached() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mAttachedCompositor = true;

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
                        if (mCompositor.layerView != null) {
                            mCompositor.layerView.onCompositorReady();
                        }
                    }
                });
                break;
            }

            case FIRST_PAINT: {
                if (mCompositor.layerView != null) {
                    mCompositor.layerView.setSurfaceBackgroundColor(Color.TRANSPARENT);
                }
                break;
            }

            case LAYERS_UPDATED: {
                if (mCompositor.layerView != null) {
                    mCompositor.layerView.notifyDrawListeners();
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

    /* package */ boolean isCompositorReady() {
        return mCompositorReady;
    }

    /* package */ void onCompositorReady() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        mCompositorReady = true;

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

    /* package */ void onMetricsChanged(final float scrollX, final float scrollY,
                                        final float zoom) {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mViewportMetrics.viewportRectLeft != scrollX ||
            mViewportMetrics.viewportRectTop != scrollY) {
            mViewportMetrics = mViewportMetrics.setViewportOrigin(scrollX, scrollY);
        }

        if (mViewportMetrics.zoomFactor != zoom) {
            mViewportMetrics = mViewportMetrics.setZoomFactor(zoom);
        }
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

        if (mAttachedCompositor) {
            mCompositor.onBoundsChanged(mLeft, mTop + toolbarHeight,
                                        mWidth, mHeight - toolbarHeight);
        }

        if (mViewportMetrics.viewportRectWidth != mWidth ||
            mViewportMetrics.viewportRectHeight != (mHeight - toolbarHeight)) {
            mViewportMetrics = mViewportMetrics.setViewportSize(mWidth,
                                                                mHeight - toolbarHeight);
        }

        if (mCompositor.layerView != null) {
            mCompositor.layerView.onSizeChanged(mWidth, mHeight);
        }
    }

    /* package */ void onSurfaceChanged(final Surface surface, final int width,
                                        final int height) {
        ThreadUtils.assertOnUiThread();

        mWidth = width;
        mHeight = height;

        if (mViewportMetrics.viewportRectWidth == 0 ||
            mViewportMetrics.viewportRectHeight == 0) {
            mViewportMetrics = mViewportMetrics.setViewportSize(width, height);
        }

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

    public void addDisplay(final GeckoDisplay display) {
        ThreadUtils.assertOnUiThread();

        if (display.getListener() != null) {
            throw new IllegalArgumentException("Display already attached");
        } else if (mDisplay != null) {
            throw new IllegalArgumentException("Only one display supported");
        }

        mDisplay = display;
        display.setListener(new GeckoDisplay.Listener() {
            @Override
            public void surfaceChanged(final Surface surface, final int width,
                                       final int height) {
                onSurfaceChanged(surface, width, height);
            }

            @Override
            public void surfaceDestroyed() {
                onSurfaceDestroyed();
            }

            @Override
            public void screenOriginChanged(final int left, final int top) {
                onScreenOriginChanged(left, top);
            }
        });
    }

    public void removeDisplay(final GeckoDisplay display) {
        ThreadUtils.assertOnUiThread();

        if (mDisplay != display) {
            throw new IllegalArgumentException("Display not attached");
        }

        display.setListener(null);
        mDisplay = null;
    }
}
