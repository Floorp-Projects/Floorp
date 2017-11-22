/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.util.Log;
import android.view.Surface;

public class LayerSession {
    private static final String LOGTAG = "GeckoLayerSession";
    private static final boolean DEBUG = false;

    // Sent from compositor when the static toolbar image has been updated and
    // is ready to animate.
    /* package */ final static int STATIC_TOOLBAR_READY = 1;
    // Sent from compositor when the static toolbar has been made visible so
    // the real toolbar should be shown.
    /* package */ final static int TOOLBAR_SHOW = 4;
    // Special message sent from UiCompositorControllerChild once it is open.
    /* package */ final static int COMPOSITOR_CONTROLLER_OPEN = 20;
    // Special message sent from controller to query if the compositor controller is open.
    /* package */ final static int IS_COMPOSITOR_CONTROLLER_OPEN = 21;

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

        @WrapForJNI(calledFrom = "any", dispatchTo = "current")
        public native void setPinned(boolean pinned, int reason);

        @WrapForJNI(calledFrom = "any", dispatchTo = "current")
        public native void sendToolbarAnimatorMessage(int message);

        @WrapForJNI(calledFrom = "ui")
        private void recvToolbarAnimatorMessage(int message) {
            if (message == COMPOSITOR_CONTROLLER_OPEN) {
                if (LayerSession.this.isCompositorReady()) {
                    return;
                }

                // Delay calling onCompositorReady to avoid deadlock due
                // to synchronous call to the compositor.
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        LayerSession.this.onCompositorReady();
                    }
                });
            }

            if (layerView != null) {
                layerView.handleToolbarAnimatorMessage(message);
            }

            if (message == STATIC_TOOLBAR_READY || message == TOOLBAR_SHOW) {
                LayerSession.this.onWindowBoundsChanged();
            }
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
        public void updateRootFrameMetrics(float scrollX, float scrollY, float zoom) {
            if (layerView != null) {
                layerView.onMetricsChanged(scrollX, scrollY, zoom);
            }
        }
    }

    protected final Compositor mCompositor = new Compositor();

    // Following fields are accessed on UI thread.
    private GeckoDisplay mDisplay;
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
    }

    /* protected */ void onWindowBoundsChanged() {
        if (DEBUG) {
            ThreadUtils.assertOnUiThread();
        }

        if (mAttachedCompositor) {
            final int toolbarHeight;
            if (mCompositor.layerView != null) {
                toolbarHeight = mCompositor.layerView.getCurrentToolbarHeight();
            } else {
                toolbarHeight = 0;
            }
            mCompositor.onBoundsChanged(mLeft, mTop + toolbarHeight,
                                        mWidth, mHeight - toolbarHeight);

            if (mCompositor.layerView != null) {
                mCompositor.layerView.onSizeChanged(mWidth, mHeight);
            }
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
