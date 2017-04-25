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
import org.mozilla.gecko.GeckoAccessibility;
import org.mozilla.gecko.GeckoAppShell;
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
import android.os.Parcelable;
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
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * A view rendered by the layer compositor.
 */
public class LayerView extends FrameLayout {
    private static final String LOGTAG = "GeckoLayerView";

    private GeckoLayerClient mLayerClient;
    private PanZoomController mPanZoomController;
    private DynamicToolbarAnimator mToolbarAnimator;
    private FullScreenState mFullScreenState;

    private SurfaceView mSurfaceView;
    private TextureView mTextureView;

    private Listener mListener;

    /* This should only be modified on the Java UI thread. */
    private final Overscroll mOverscroll;

    private boolean mServerSurfaceValid;
    private int mWidth, mHeight;

    private boolean onAttachedToWindowCalled;
    private int mDefaultClearColor = Color.WHITE;
    /* package */ GetPixelsResult mGetPixelsResult;
    private final List<DrawListener> mDrawListeners;

    /* This is written by the Gecko thread and the UI thread, and read by the UI thread. */
    @WrapForJNI(stubName = "CompositorCreated", calledFrom = "ui")
    /* package */ volatile boolean mCompositorCreated;

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
    /* package */ final static int IS_COMPOSITOR_CONTROLLER_OPEN    = 21; // Special message sent from controller to query if the compositor controller is open

    private void postCompositorMessage(final int message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mCompositor.sendToolbarAnimatorMessage(message);
            }
        });
    }

    /* package */ class Compositor extends JNIObject {
        public Compositor() {
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko")
        @Override protected native void disposeNative();

        // Gecko thread sets its Java instances; does not block UI thread.
        @WrapForJNI(calledFrom = "any", dispatchTo = "gecko")
        /* package */ native void attachToJava(GeckoLayerClient layerClient,
                                               NativePanZoomController npzc);

        @WrapForJNI(calledFrom = "any", dispatchTo = "gecko_priority")
        /* package */ native void onSizeChanged(int windowWidth, int windowHeight,
                                                int screenWidth, int screenHeight);

        // Gecko thread creates compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "proxy")
        /* package */ native void createCompositor(int width, int height, Object surface);

        // Gecko thread pauses compositor; blocks UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void syncPauseCompositor();

        // UI thread resumes compositor and notifies Gecko thread; does not block UI thread.
        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void syncResumeResizeCompositor(int width, int height, Object surface);

        @WrapForJNI(calledFrom = "any", dispatchTo = "current")
        /* package */ native void syncInvalidateAndScheduleComposite();

        @WrapForJNI(calledFrom = "any", dispatchTo = "current")
        /* package */ native void setMaxToolbarHeight(int height);

        @WrapForJNI(calledFrom = "any", dispatchTo = "current")
        /* package */ native void setPinned(boolean pinned, int reason);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void sendToolbarAnimatorMessage(int message);

        @WrapForJNI(calledFrom = "ui")
        /* package */ void recvToolbarAnimatorMessage(int message) {
            handleToolbarAnimatorMessage(message);
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void setDefaultClearColor(int color);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void requestScreenPixels();

        @WrapForJNI(calledFrom = "ui")
        /* package */ void recvScreenPixels(int width, int height, int[] pixels) {
            if (mGetPixelsResult != null) {
                mGetPixelsResult.onPixelsResult(width, height, IntBuffer.wrap(pixels));
                mGetPixelsResult = null;
            }
        }

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void enableLayerUpdateNotifications(boolean enable);

        @WrapForJNI(calledFrom = "ui", dispatchTo = "current")
        /* package */ native void sendToolbarPixelsToCompositor(final int width, final int height, final int[] pixels);

        @WrapForJNI(calledFrom = "gecko")
        private void reattach() {
            mCompositorCreated = true;
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // Make sure it is still valid
                    if (mCompositorCreated) {
                        mCompositor.setDefaultClearColor(mDefaultClearColor);
                        mCompositor.enableLayerUpdateNotifications(!mDrawListeners.isEmpty());
                        mToolbarAnimator.notifyCompositorCreated(mCompositor);
                    }
                }
            });
        }

        @WrapForJNI(calledFrom = "gecko")
        private void destroy() {
            // The nsWindow has been closed. First mark our compositor as destroyed.
            LayerView.this.mCompositorCreated = false;

            LayerView.this.mLayerClient.setGeckoReady(false);

            // Then clear out any pending calls on the UI thread by disposing on the UI thread.
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    LayerView.this.mToolbarAnimator.notifyCompositorDestroyed();
                    LayerView.this.mDrawListeners.clear();
                    disposeNative();
                }
            });
        }
    }

    /* package */ void handleToolbarAnimatorMessage(int message) {
        switch(message) {
            case STATIC_TOOLBAR_NEEDS_UPDATE:
                // Send updated toolbar image to compositor.
                Bitmap bm = mToolbarAnimator.getBitmapOfToolbarChrome();
                if (bm == null) {
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
                mListener.surfaceChanged();
                postCompositorMessage(TOOLBAR_HIDDEN);
                break;
            case TOOLBAR_SHOW:
                // Show toolbar.
                mToolbarAnimator.onToggleChrome(true);
                mListener.surfaceChanged();
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
                mToolbarAnimator.notifyCompositorControllerOpen();
                break;
            default:
                Log.e(LOGTAG,"Unhandled Toolbar Animator Message: " + message);
                break;
        }
    }

    private final Compositor mCompositor = new Compositor();

    public boolean shouldUseTextureView() {
        // Disable TextureView support for now as it causes panning/zooming
        // performance regressions (see bug 792259). Uncomment the code below
        // once this bug is fixed.
        return false;

        /*
        // we can only use TextureView on ICS or higher
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            Log.i(LOGTAG, "Not using TextureView: not on ICS+");
            return false;
        }

        try {
            // and then we can only use it if we have a hardware accelerated window
            Method m = View.class.getMethod("isHardwareAccelerated", (Class[]) null);
            return (Boolean) m.invoke(this);
        } catch (Exception e) {
            Log.i(LOGTAG, "Not using TextureView: caught exception checking for hw accel: " + e.toString());
            return false;
        } */
    }

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
        mLayerClient = new GeckoLayerClient(getContext(), this);
        if (mOverscroll != null) {
            mLayerClient.setOverscrollHandler(mOverscroll);
        }

        mPanZoomController = mLayerClient.getPanZoomController();
        mToolbarAnimator = mLayerClient.getDynamicToolbarAnimator();

        setFocusable(true);
        setFocusableInTouchMode(true);

        GeckoAccessibility.setDelegate(this);
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

    public void showSurface() {
        // Fix this if TextureView support is turned back on above
        mSurfaceView.setVisibility(View.VISIBLE);
    }

    public void hideSurface() {
        // Fix this if TextureView support is turned back on above
        mSurfaceView.setVisibility(View.INVISIBLE);
    }

    public void destroy() {
        if (mLayerClient != null) {
            mLayerClient.destroy();
        }
    }

    @Override
    public void dispatchDraw(final Canvas canvas) {
        super.dispatchDraw(canvas);

        // We must have a layer client to get valid viewport metrics
        if (mLayerClient != null && mOverscroll != null) {
            mOverscroll.draw(canvas, getViewportMetrics());
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            requestFocus();
        }

        if (!mLayerClient.isGeckoReady()) {
            // If gecko isn't loaded yet, don't try sending events to the
            // native code because it's just going to crash
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onTouchEvent(event)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        // If we get a touchscreen hover event, and accessibility is not enabled,
        // don't send it to gecko.
        if (event.getSource() == InputDevice.SOURCE_TOUCHSCREEN &&
            !GeckoAccessibility.isEnabled()) {
            return false;
        }

        if (!mLayerClient.isGeckoReady()) {
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
        if (!mLayerClient.isGeckoReady()) {
            // If gecko isn't loaded yet, don't try sending events to the
            // native code because it's just going to crash
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onMotionEvent(event)) {
            return true;
        }
        return false;
    }

    @Override
    protected void onRestoreInstanceState(final Parcelable state) {
        if (onAttachedToWindowCalled) {
            attachCompositor();
        }
        super.onRestoreInstanceState(state);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        // We are adding descendants to this LayerView, but we don't want the
        // descendants to affect the way LayerView retains its focus.
        setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);

        // This check should not be done before the view is attached to a window
        // as hardware acceleration will not be enabled at that point.
        // We must create and add the SurfaceView instance before the view tree
        // is fully created to avoid flickering (see bug 801477).
        if (shouldUseTextureView()) {
            mTextureView = new TextureView(getContext());
            mTextureView.setSurfaceTextureListener(new SurfaceTextureListener());

            // The background is set to this color when the LayerView is
            // created, and it will be shown immediately at startup. Shortly
            // after, the tab's background color will be used before any content
            // is shown.
            mTextureView.setBackgroundColor(Color.WHITE);
            addView(mTextureView, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        } else {
            // This will stop PropertyAnimator from creating a drawing cache (i.e. a bitmap)
            // from a SurfaceView, which is just not possible (the bitmap will be transparent).
            setWillNotCacheDrawing(false);

            mSurfaceView = new LayerSurfaceView(getContext(), this);
            mSurfaceView.setBackgroundColor(Color.WHITE);
            addView(mSurfaceView, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

            SurfaceHolder holder = mSurfaceView.getHolder();
            holder.addCallback(new SurfaceListener());
        }

        attachCompositor();

        onAttachedToWindowCalled = true;
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        onAttachedToWindowCalled = false;
    }

    // Don't expose GeckoLayerClient to things outside this package; only expose it as an Object
    GeckoLayerClient getLayerClient() { return mLayerClient; }

    /* package */ boolean isGeckoReady() {
        return mLayerClient.isGeckoReady();
    }

    public PanZoomController getPanZoomController() { return mPanZoomController; }
    public DynamicToolbarAnimator getDynamicToolbarAnimator() { return mToolbarAnimator; }

    public ImmutableViewportMetrics getViewportMetrics() {
        return mLayerClient.getViewportMetrics();
    }

    public Matrix getMatrixForLayerRectToViewRect() {
        return mLayerClient.getMatrixForLayerRectToViewRect();
    }

    public void setSurfaceBackgroundColor(int newColor) {
        if (mSurfaceView != null) {
            mSurfaceView.setBackgroundColor(newColor);
        }
    }

    public void requestRender() {
        if (mCompositorCreated) {
            mCompositor.syncInvalidateAndScheduleComposite();
        }
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

        if (mCompositorCreated) {
            mGetPixelsResult = getPixelsResult;
            mCompositor.requestScreenPixels();
        } else {
            getPixelsResult.onPixelsResult(0, 0, null);
        }
    }

    public void setListener(Listener listener) {
        mListener = listener;
    }

    Listener getListener() {
        return mListener;
    }

    private void attachCompositor() {
        final NativePanZoomController npzc = (NativePanZoomController) mPanZoomController;

        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            mCompositor.attachToJava(mLayerClient, npzc);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY,
                    mCompositor, "attachToJava",
                    GeckoLayerClient.class, mLayerClient,
                    NativePanZoomController.class, npzc);
        }
    }

    @WrapForJNI(calledFrom = "ui")
    protected Object getCompositor() {
        return mCompositor;
    }

    void serverSurfaceChanged(int newWidth, int newHeight) {
        ThreadUtils.assertOnUiThread();

        mWidth = newWidth;
        mHeight = newHeight;
        mServerSurfaceValid = true;

        updateCompositor();
    }

    void updateCompositor() {
        ThreadUtils.assertOnUiThread();

        if (mCompositorCreated) {
            // If the compositor has already been created, just resume it instead. We don't need
            // to block here because if the surface is destroyed before the compositor grabs it,
            // we can handle that gracefully (i.e. the compositor will remain paused).
            if (!mServerSurfaceValid) {
                return;
            }
            // Asking Gecko to resume the compositor takes too long (see
            // https://bugzilla.mozilla.org/show_bug.cgi?id=735230#c23), so we
            // resume the compositor directly. We still need to inform Gecko about
            // the compositor resuming, so that Gecko knows that it can now draw.
            // It is important to not notify Gecko until after the compositor has
            // been resumed, otherwise Gecko may send updates that get dropped.
            mCompositor.syncResumeResizeCompositor(mWidth, mHeight, getSurface());
            return;
        }

        // Only try to create the compositor if we have a valid surface and gecko is up. When these
        // two conditions are satisfied, we can be relatively sure that the compositor creation will
        // happen without needing to block anywhere.
        if (mServerSurfaceValid && getLayerClient().isGeckoReady()) {
            mCompositorCreated = true;
            mCompositor.createCompositor(mWidth, mHeight, getSurface());
            mCompositor.setDefaultClearColor(mDefaultClearColor);
            mCompositor.enableLayerUpdateNotifications(!mDrawListeners.isEmpty());
            mToolbarAnimator.notifyCompositorCreated(mCompositor);
        }
    }

    /* When using a SurfaceView (mSurfaceView != null), resizing happens in two
     * phases. First, the LayerView changes size, then, often some frames later,
     * the SurfaceView changes size. Because of this, we need to split the
     * resize into two phases to avoid jittering.
     *
     * The first phase is the LayerView size change. mListener is notified so
     * that a synchronous draw can be performed (otherwise a blank frame will
     * appear).
     *
     * The second phase is the SurfaceView size change. At this point, the
     * backing GL surface is resized and another synchronous draw is performed.
     * Gecko is also sent the new window size, and this will likely cause an
     * extra draw a few frames later, after it's re-rendered and caught up.
     *
     * In the case that there is no valid GL surface (for example, when
     * resuming, or when coming back from the awesomescreen), or we're using a
     * TextureView instead of a SurfaceView, the first phase is skipped.
     */
    private void onSizeChanged(int width, int height) {
        if (!mServerSurfaceValid || mSurfaceView == null) {
            surfaceChanged(width, height);
            return;
        }

        if (mCompositorCreated) {
            mCompositor.syncResumeResizeCompositor(width, height, getSurface());
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(width, height);
        }
    }

    private void surfaceChanged(int width, int height) {
        serverSurfaceChanged(width, height);

        if (mListener != null) {
            mListener.surfaceChanged();
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(width, height);
        }
    }

    void notifySizeChanged(int windowWidth, int windowHeight, int screenWidth, int screenHeight) {
        mCompositor.onSizeChanged(windowWidth, windowHeight, screenWidth, screenHeight);
    }

    void serverSurfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        // We need to coordinate with Gecko when pausing composition, to ensure
        // that Gecko never executes a draw event while the compositor is paused.
        // This is sent synchronously to make sure that we don't attempt to use
        // any outstanding Surfaces after we call this (such as from a
        // serverSurfaceDestroyed notification), and to make sure that any in-flight
        // Gecko draw events have been processed.  When this returns, composition is
        // definitely paused -- it'll synchronize with the Gecko event loop, which
        // in turn will synchronize with the compositor thread.
        if (mCompositorCreated) {
            mCompositor.syncPauseCompositor();
        }

        mServerSurfaceValid = false;
    }

    private void onDestroyed() {
        serverSurfaceDestroyed();
    }

    public Object getNativeWindow() {
        if (mSurfaceView != null)
            return mSurfaceView.getHolder();

        return mTextureView.getSurfaceTexture();
    }

    public Object getSurface() {
      if (mSurfaceView != null) {
        return mSurfaceView.getHolder().getSurface();
      }
      return null;
    }

    public interface Listener {
        void surfaceChanged();
    }

    private class SurfaceListener implements SurfaceHolder.Callback {
        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width,
                                                int height) {
            onSizeChanged(width, height);
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            onDestroyed();
        }
    }

    /* A subclass of SurfaceView to listen to layout changes, as
     * View.OnLayoutChangeListener requires API level 11.
     */
    private class LayerSurfaceView extends SurfaceView {
        private LayerView mParent;

        public LayerSurfaceView(Context context, LayerView parent) {
            super(context);
            mParent = parent;
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            if (changed && mParent.mServerSurfaceValid) {
                mParent.surfaceChanged(right - left, bottom - top);
            }
        }
    }

    private class SurfaceTextureListener implements TextureView.SurfaceTextureListener {
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
            // We don't do this for surfaceCreated above because it is always followed by a surfaceChanged,
            // but that is not the case here.
            onSizeChanged(width, height);
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            onDestroyed();
            return true; // allow Android to call release() on the SurfaceTexture, we are done drawing to it
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
            onSizeChanged(width, height);
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surface) {

        }
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
        if (mCompositorCreated && wasEmpty) {
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
        if (mCompositorCreated && notEmpty && mDrawListeners.isEmpty()) {
            mCompositor.enableLayerUpdateNotifications(false);
        }
    }

    @RobocopTarget
    public static interface DrawListener {
        public void drawFinished();
    }

    @Override
    public void setOverScrollMode(int overscrollMode) {
        super.setOverScrollMode(overscrollMode);
    }

    @Override
    public int getOverScrollMode() {
        return super.getOverScrollMode();
    }

    public float getZoomFactor() {
        return getLayerClient().getViewportMetrics().zoomFactor;
    }

    @Override
    public void onFocusChanged (boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        GeckoAccessibility.onLayerViewFocusChanged(gainFocus);
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
        if (mCompositorCreated) {
            mCompositor.setDefaultClearColor(mDefaultClearColor);
        }
   }
}
