/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;

import org.mozilla.gecko.AndroidGamepadManager;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.support.v4.util.SimpleArrayMap;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.InputDevice;
import android.view.accessibility.AccessibilityManager;
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * A view rendered by the layer compositor.
 */
public class LayerView extends FrameLayout {
    private static final String LOGTAG = "GeckoLayerView";

    private static AccessibilityManager sAccessibilityManager;

    private PanZoomController mPanZoomController;
    private DynamicToolbarAnimator mToolbarAnimator;
    private FullScreenState mFullScreenState;

    // Accessed on UI thread.
    private ImmutableViewportMetrics mViewportMetrics;

    /* This should only be modified on the Java UI thread. */
    private final Overscroll mOverscroll;

    private int mDefaultClearColor = Color.WHITE;
    /* package */ GetPixelsResult mGetPixelsResult;
    private final List<DrawListener> mDrawListeners;

    //
    // NOTE: These values are also defined in gfx/layers/ipc/UiCompositorControllerMessageTypes.h
    //       and must be kept in sync. Any new AnimatorMessageType added here must also be added there.
    //
    /* package */ final static int STATIC_TOOLBAR_NEEDS_UPDATE      = 0;  // Sent from compositor when the static toolbar wants to hide.
    /* package */ final static int STATIC_TOOLBAR_READY             = 1;  // Sent from compositor when the static toolbar image has been updated and is ready to animate.
    /* package */ final static int TOOLBAR_HIDDEN                   = 2;  // Sent to compositor when the real toolbar has been hidden.
    /* package */ final static int TOOLBAR_VISIBLE                  = 3;  // Sent to compositor when the real toolbar is visible.
    /* package */ final static int TOOLBAR_SHOW                     = 4;  // Sent from compositor when the static toolbar has been made visible so the real toolbar should be shown.
    /* package */ final static int FIRST_PAINT                      = 5;  // Sent from compositor after first paint
    /* package */ final static int REQUEST_SHOW_TOOLBAR_IMMEDIATELY = 6;  // Sent to compositor requesting toolbar be shown immediately
    /* package */ final static int REQUEST_SHOW_TOOLBAR_ANIMATED    = 7;  // Sent to compositor requesting toolbar be shown animated
    /* package */ final static int REQUEST_HIDE_TOOLBAR_IMMEDIATELY = 8;  // Sent to compositor requesting toolbar be hidden immediately
    /* package */ final static int REQUEST_HIDE_TOOLBAR_ANIMATED    = 9;  // Sent to compositor requesting toolbar be hidden animated
    /* package */ final static int LAYERS_UPDATED                   = 10; // Sent from compositor when a layer has been updated
    /* package */ final static int TOOLBAR_SNAPSHOT_FAILED          = 11; // Sent to compositor when the toolbar snapshot fails.
    /* package */ final static int COMPOSITOR_CONTROLLER_OPEN       = 20; // Special message sent from UiCompositorControllerChild once it is open

    private void postCompositorMessage(final int message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mCompositor.sendToolbarAnimatorMessage(message);
            }
        });
    }

    /* package */ boolean isCompositorReady() {
        ThreadUtils.assertOnUiThread();
        return mCompositor != null && mCompositor.isReady();
    }

    /* package */ void handleToolbarAnimatorMessage(int message) {
        switch (message) {
            case STATIC_TOOLBAR_NEEDS_UPDATE:
                // Send updated toolbar image to compositor.
                Bitmap bm = mToolbarAnimator.getBitmapOfToolbarChrome();
                if (bm == null) {
                    postCompositorMessage(TOOLBAR_SNAPSHOT_FAILED);
                    break;
                }
                final int width = bm.getWidth();
                final int height = bm.getHeight();
                int[] pixels = new int[bm.getByteCount() / 4];
                try {
                    bm.getPixels(pixels, /* offset */ 0, /* stride */ width, /* x */ 0, /* y */ 0, width, height);
                    mCompositor.sendToolbarPixelsToCompositor(width, height, pixels);
                } catch (Exception e) {
                    Log.e(LOGTAG, "Caught exception while getting toolbar pixels from Bitmap: " + e.toString());
                }
                break;
            case STATIC_TOOLBAR_READY:
                // Hide toolbar and send TOOLBAR_HIDDEN message to compositor
                mToolbarAnimator.onToggleChrome(false);
                adjustViewportSize();
                postCompositorMessage(TOOLBAR_HIDDEN);
                break;
            case TOOLBAR_SHOW:
                // Show toolbar.
                mToolbarAnimator.onToggleChrome(true);
                adjustViewportSize();
                postCompositorMessage(TOOLBAR_VISIBLE);
                break;
            case FIRST_PAINT:
                setSurfaceBackgroundColor(Color.TRANSPARENT);
                break;
            case LAYERS_UPDATED:
                for (DrawListener listener : mDrawListeners) {
                    listener.drawFinished();
                }
                break;
            case COMPOSITOR_CONTROLLER_OPEN:
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mCompositor.setDefaultClearColor(mDefaultClearColor);
                        mCompositor.enableLayerUpdateNotifications(!mDrawListeners.isEmpty());
                        mToolbarAnimator.updateCompositor();
                    }
                });
                break;
            default:
                Log.e(LOGTAG, "Unhandled Toolbar Animator Message: " + message);
                break;
        }
    }

    /* package */ void onCompositorReady() {
        mCompositor.setDefaultClearColor(mDefaultClearColor);
        mCompositor.enableLayerUpdateNotifications(!mDrawListeners.isEmpty());
    }

    /* protected */ LayerSession mSession;
    private LayerSession.Compositor mCompositor;

    public LayerView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mFullScreenState = FullScreenState.NONE;

        mOverscroll = new OverscrollEdgeEffect(this);
        mDrawListeners = new ArrayList<DrawListener>();
    }

    public LayerView(Context context) {
        this(context, null);
    }

    public void initializeView() {
        mToolbarAnimator = new DynamicToolbarAnimator(this);
        mPanZoomController = PanZoomController.Factory.create(this);
        if (mOverscroll != null) {
            mPanZoomController.setOverscrollHandler(mOverscroll);
        }

        mViewportMetrics = new ImmutableViewportMetrics()
                .setViewportSize(getWidth(), getHeight());
    }

    /**
     * MotionEventHelper dragAsync() robocop tests can instruct
     * PanZoomController not to generate longpress events.
     * This call comes in from a thread other than the UI thread.
     * So dispatch to UI thread first to prevent assert in nsWindow.
     */
    public void setIsLongpressEnabled(final boolean isLongpressEnabled) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mPanZoomController.setIsLongpressEnabled(isLongpressEnabled);
            }
        });
    }

    private static Point getEventRadius(MotionEvent event) {
        return new Point((int)event.getToolMajor() / 2,
                         (int)event.getToolMinor() / 2);
    }

    protected void destroy() {
        if (mPanZoomController != null) {
            mPanZoomController.destroy();
        }
    }

    @Override
    public void dispatchDraw(final Canvas canvas) {
        super.dispatchDraw(canvas);

        // We must have a layer client to get valid viewport metrics
        if (mOverscroll != null) {
            mOverscroll.draw(canvas, mSession.getViewportMetrics());
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            requestFocus();
        }

        if (!isCompositorReady()) {
            // If gecko isn't loaded yet, don't try sending events to the
            // native code because it's just going to crash
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onTouchEvent(event)) {
            return true;
        }
        return false;
    }

    private boolean isAccessibilityEnabled() {
        if (sAccessibilityManager == null) {
            sAccessibilityManager = (AccessibilityManager)
                    getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        }
        return sAccessibilityManager.isEnabled() &&
               sAccessibilityManager.isTouchExplorationEnabled();
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        // If we get a touchscreen hover event, and accessibility is not enabled,
        // don't send it to gecko.
        if (event.getSource() == InputDevice.SOURCE_TOUCHSCREEN &&
            !isAccessibilityEnabled()) {
            return false;
        }

        if (!isCompositorReady()) {
            // If gecko isn't loaded yet, don't try sending events to the
            // native code because it's just going to crash
            return true;
        } else if (mPanZoomController != null && mPanZoomController.onMotionEvent(event)) {
            return true;
        }

        return false;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (AndroidGamepadManager.handleMotionEvent(event)) {
            return true;
        }
        if (!isCompositorReady()) {
            // If gecko isn't loaded yet, don't try sending events to the
            // native code because it's just going to crash
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onMotionEvent(event)) {
            return true;
        }
        return false;
    }

    public PanZoomController getPanZoomController() { return mPanZoomController; }
    public DynamicToolbarAnimator getDynamicToolbarAnimator() { return mToolbarAnimator; }

    public ImmutableViewportMetrics getViewportMetrics() {
        return mViewportMetrics;
    }

    public void setSurfaceBackgroundColor(int newColor) {
    }

    public interface GetPixelsResult {
        public void onPixelsResult(int width, int height, IntBuffer pixels);
    }

    @RobocopTarget
    public void getPixels(final GetPixelsResult getPixelsResult) {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    getPixels(getPixelsResult);
                }
            });
            return;
        }

        if (isCompositorReady()) {
            mGetPixelsResult = getPixelsResult;
            mCompositor.requestScreenPixels();
        } else {
            getPixelsResult.onPixelsResult(0, 0, null);
        }
    }

    /* package */ void recvScreenPixels(int width, int height, int[] pixels) {
        if (mGetPixelsResult != null) {
            mGetPixelsResult.onPixelsResult(width, height, IntBuffer.wrap(pixels));
            mGetPixelsResult = null;
        }
    }

    protected void attachCompositor(final LayerSession session) {
        mSession = session;
        mCompositor = session.mCompositor;
        mCompositor.layerView = this;

        mToolbarAnimator.notifyCompositorCreated(mCompositor);

        final NativePanZoomController npzc = (NativePanZoomController) mPanZoomController;

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            mCompositor.attachToJava(npzc);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    mCompositor, "attachToJava",
                    NativePanZoomController.class, npzc);
        }
    }

    @WrapForJNI(calledFrom = "ui")
    private Object getCompositor() {
        return isCompositorReady() ? mCompositor : null;
    }

    private void adjustViewportSize() {
        final IntSize viewportSize = mToolbarAnimator.getViewportSize();

        if (mViewportMetrics.viewportRectWidth == viewportSize.width &&
            mViewportMetrics.viewportRectHeight == viewportSize.height) {
            return;
        }

        mViewportMetrics = mViewportMetrics.setViewportSize(viewportSize.width,
                                                            viewportSize.height);
    }

    /* package */ void onSizeChanged(int width, int height) {
        adjustViewportSize();

        if (mOverscroll != null) {
            mOverscroll.setSize(width, height);
        }
    }

    /* package */ void onMetricsChanged(final float scrollX, final float scrollY,
                                        final float zoom) {
        mViewportMetrics = mViewportMetrics.setViewportOrigin(scrollX, scrollY)
                                           .setZoomFactor(zoom);
        mToolbarAnimator.onMetricsChanged(mViewportMetrics);
    }

    @RobocopTarget
    public void addDrawListener(final DrawListener listener) {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    addDrawListener(listener);
                }
            });
            return;
        }

        boolean wasEmpty = mDrawListeners.isEmpty();
        mDrawListeners.add(listener);
        if (isCompositorReady() && wasEmpty) {
            mCompositor.enableLayerUpdateNotifications(true);
        }
    }

    @RobocopTarget
    public void removeDrawListener(final DrawListener listener) {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    removeDrawListener(listener);
                }
            });
            return;
        }

        boolean notEmpty = mDrawListeners.isEmpty();
        mDrawListeners.remove(listener);
        if (isCompositorReady() && notEmpty && mDrawListeners.isEmpty()) {
            mCompositor.enableLayerUpdateNotifications(false);
        }
    }

    /* package */ void clearDrawListeners() {
        mDrawListeners.clear();
    }

    /* package */ void notifyDrawListeners() {
        for (final DrawListener listener : mDrawListeners) {
            listener.drawFinished();
        }
    }

    @RobocopTarget
    public static interface DrawListener {
        public void drawFinished();
    }

    public float getZoomFactor() {
        return getViewportMetrics().zoomFactor;
    }

    public void setFullScreenState(FullScreenState state) {
        mFullScreenState = state;
    }

    public boolean isFullScreen() {
        return mFullScreenState != FullScreenState.NONE;
    }

    public void setMaxToolbarHeight(int maxHeight) {
        mToolbarAnimator.setMaxToolbarHeight(maxHeight);
    }

    public int getCurrentToolbarHeight() {
        return mToolbarAnimator.getCurrentToolbarHeight();
    }

    public void setClearColor(final int color) {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    setClearColor(color);
                }
            });
            return;
        }

        mDefaultClearColor = color;
        if (isCompositorReady()) {
            mCompositor.setDefaultClearColor(mDefaultClearColor);
        }
    }

    public boolean isIMEEnabled() {
        return false;
    }
}
