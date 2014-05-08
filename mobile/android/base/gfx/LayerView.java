/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.AndroidGamepadManager;
import org.mozilla.gecko.GeckoAccessibility;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.TouchEventInterceptor;
import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.EventDispatcher;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import java.nio.IntBuffer;
import java.util.ArrayList;

/**
 * A view rendered by the layer compositor.
 *
 * Note that LayerView is accessed by Robocop via reflection.
 */
public class LayerView extends FrameLayout implements Tabs.OnTabsChangedListener {
    private static String LOGTAG = "GeckoLayerView";

    private GeckoLayerClient mLayerClient;
    private PanZoomController mPanZoomController;
    private LayerMarginsAnimator mMarginsAnimator;
    private GLController mGLController;
    private InputConnectionHandler mInputConnectionHandler;
    private LayerRenderer mRenderer;
    /* Must be a PAINT_xxx constant */
    private int mPaintState;
    private int mBackgroundColor;
    private FullScreenState mFullScreenState;

    private SurfaceView mSurfaceView;
    private TextureView mTextureView;

    private Listener mListener;

    /* This should only be modified on the Java UI thread. */
    private final ArrayList<TouchEventInterceptor> mTouchInterceptors;
    private final Overscroll mOverscroll;

    /* Flags used to determine when to show the painted surface. */
    public static final int PAINT_START = 0;
    public static final int PAINT_BEFORE_FIRST = 1;
    public static final int PAINT_AFTER_FIRST = 2;

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

        mGLController = GLController.getInstance(this);
        mPaintState = PAINT_START;
        mBackgroundColor = Color.WHITE;
        mFullScreenState = FullScreenState.NONE;

        mTouchInterceptors = new ArrayList<TouchEventInterceptor>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            mOverscroll = new OverscrollEdgeEffect(this);
        } else {
            mOverscroll = null;
        }
        Tabs.registerOnTabsChangedListener(this);
    }

    public LayerView(Context context) {
        this(context, null);
    }

    public void initializeView(EventDispatcher eventDispatcher) {
        mLayerClient = new GeckoLayerClient(getContext(), this, eventDispatcher);
        if (mOverscroll != null) {
            mLayerClient.setOverscrollHandler(mOverscroll);
        }

        mPanZoomController = mLayerClient.getPanZoomController();
        mMarginsAnimator = mLayerClient.getLayerMarginsAnimator();

        mRenderer = new LayerRenderer(this);
        mInputConnectionHandler = null;

        setFocusable(true);
        setFocusableInTouchMode(true);

        GeckoAccessibility.setDelegate(this);
    }

    private Point getEventRadius(MotionEvent event) {
        if (Build.VERSION.SDK_INT >= 9) {
            return new Point((int)event.getToolMajor()/2,
                             (int)event.getToolMinor()/2);
        }

        float size = event.getSize();
        DisplayMetrics displaymetrics = getContext().getResources().getDisplayMetrics();
        size = size * Math.min(displaymetrics.heightPixels, displaymetrics.widthPixels);
        return new Point((int)size, (int)size);
    }

    public void geckoConnected() {
        // See if we want to force 16-bit colour before doing anything
        PrefsHelper.getPref("gfx.android.rgb16.force", new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, boolean force16bit) {
                if (force16bit) {
                    GeckoAppShell.setScreenDepthOverride(16);
                }
            }
        });

        mLayerClient.notifyGeckoReady();
        addTouchInterceptor(new TouchEventInterceptor() {
            private PointF mInitialTouchPoint = null;

            @Override
            public boolean onInterceptTouchEvent(View view, MotionEvent event) {
                return false;
            }

            @Override
            public boolean onTouch(View view, MotionEvent event) {
                if (event == null) {
                    return true;
                }

                int action = event.getActionMasked();
                PointF point = new PointF(event.getX(), event.getY());
                if (action == MotionEvent.ACTION_DOWN) {
                    mInitialTouchPoint = point;
                }

                if (mInitialTouchPoint != null && action == MotionEvent.ACTION_MOVE) {
                    Point p = getEventRadius(event);

                    if (PointUtils.subtract(point, mInitialTouchPoint).length() <
                        Math.max(PanZoomController.CLICK_THRESHOLD, Math.min(Math.min(p.x, p.y), PanZoomController.PAN_THRESHOLD))) {
                        // Don't send the touchmove event if if the users finger hasn't moved far.
                        // Necessary for Google Maps to work correctly. See bug 771099.
                        return true;
                    } else {
                        mInitialTouchPoint = null;
                    }
                }

                GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event, false));
                return true;
            }
        });
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
        if (mRenderer != null) {
            mRenderer.destroy();
        }
        Tabs.unregisterOnTabsChangedListener(this);
    }

    public void addTouchInterceptor(final TouchEventInterceptor aTouchInterceptor) {
        post(new Runnable() {
            @Override
            public void run() {
                mTouchInterceptors.add(aTouchInterceptor);
            }
        });
    }

    public void removeTouchInterceptor(final TouchEventInterceptor aTouchInterceptor) {
        post(new Runnable() {
            @Override
            public void run() {
                mTouchInterceptors.remove(aTouchInterceptor);
            }
        });
    }

    private boolean runTouchInterceptors(MotionEvent event, boolean aOnTouch) {
        boolean result = false;
        for (TouchEventInterceptor i : mTouchInterceptors) {
            if (aOnTouch) {
                result |= i.onTouch(this, event);
            } else {
                result |= i.onInterceptTouchEvent(this, event);
            }
        }

        return result;
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

        if (runTouchInterceptors(event, false)) {
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onTouchEvent(event)) {
            return true;
        }
        if (runTouchInterceptors(event, true)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        if (runTouchInterceptors(event, true)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (AndroidGamepadManager.handleMotionEvent(event)) {
            return true;
        }
        if (mPanZoomController != null && mPanZoomController.onMotionEvent(event)) {
            return true;
        }
        return false;
    }

    @Override
    protected void onAttachedToWindow() {
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
            addView(mSurfaceView, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

            SurfaceHolder holder = mSurfaceView.getHolder();
            holder.addCallback(new SurfaceListener());
        }
    }

    // Don't expose GeckoLayerClient to things outside this package; only expose it as an Object
    GeckoLayerClient getLayerClient() { return mLayerClient; }
    public Object getLayerClientObject() { return mLayerClient; }

    public PanZoomController getPanZoomController() { return mPanZoomController; }
    public LayerMarginsAnimator getLayerMarginsAnimator() { return mMarginsAnimator; }

    public ImmutableViewportMetrics getViewportMetrics() {
        return mLayerClient.getViewportMetrics();
    }

    public void abortPanning() {
        if (mPanZoomController != null) {
            mPanZoomController.abortPanning();
        }
    }

    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        return mLayerClient.convertViewPointToLayerPoint(viewPoint);
    }

    int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public void setBackgroundColor(int newColor) {
        mBackgroundColor = newColor;
        requestRender();
    }

    public void setZoomConstraints(ZoomConstraints constraints) {
        mLayerClient.setZoomConstraints(constraints);
    }

    public void setIsRTL(boolean aIsRTL) {
        mLayerClient.setIsRTL(aIsRTL);
    }

    public void setInputConnectionHandler(InputConnectionHandler inputConnectionHandler) {
        mInputConnectionHandler = inputConnectionHandler;
        mLayerClient.forceRedraw(null);
    }

    @Override
    public Handler getHandler() {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.getHandler(super.getHandler());
        return super.getHandler();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onCreateInputConnection(outAttrs);
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null && mInputConnectionHandler.onKeyPreIme(keyCode, event)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mPanZoomController != null && mPanZoomController.onKeyEvent(event)) {
            return true;
        }
        if (mInputConnectionHandler != null && mInputConnectionHandler.onKeyDown(keyCode, event)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null && mInputConnectionHandler.onKeyLongPress(keyCode, event)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (mInputConnectionHandler != null && mInputConnectionHandler.onKeyMultiple(keyCode, repeatCount, event)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null && mInputConnectionHandler.onKeyUp(keyCode, event)) {
            return true;
        }
        return false;
    }

    public boolean isIMEEnabled() {
        if (mInputConnectionHandler != null) {
            return mInputConnectionHandler.isIMEEnabled();
        }
        return false;
    }

    public void requestRender() {
        if (mListener != null) {
            mListener.renderRequested();
        }
    }

    public void addLayer(Layer layer) {
        mRenderer.addLayer(layer);
    }

    public void removeLayer(Layer layer) {
        mRenderer.removeLayer(layer);
    }

    public void postRenderTask(RenderTask task) {
        mRenderer.postRenderTask(task);
    }

    public void removeRenderTask(RenderTask task) {
        mRenderer.removeRenderTask(task);
    }

    public int getMaxTextureSize() {
        return mRenderer.getMaxTextureSize();
    }

    /** Used by robocop for testing purposes. Not for production use! */
    @RobocopTarget
    public IntBuffer getPixels() {
        return mRenderer.getPixels();
    }

    /* paintState must be a PAINT_xxx constant. */
    public void setPaintState(int paintState) {
        mPaintState = paintState;
    }

    public int getPaintState() {
        return mPaintState;
    }

    public LayerRenderer getRenderer() {
        return mRenderer;
    }

    public void setListener(Listener listener) {
        mListener = listener;
    }

    Listener getListener() {
        return mListener;
    }

    public GLController getGLController() {
        return mGLController;
    }

    private Bitmap getDrawable(String name) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        Context context = getContext();
        int resId = context.getResources().getIdentifier(name, "drawable", context.getPackageName());
        return BitmapUtils.decodeResource(context, resId, options);
    }

    Bitmap getScrollbarImage() {
        return getDrawable("scrollbar");
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
        if (!mGLController.isCompositorCreated()) {
            return;
        }

        surfaceChanged(width, height);

        if (mSurfaceView == null) {
            return;
        }

        if (mListener != null) {
            mListener.sizeChanged(width, height);
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(width, height);
        }
    }

    private void surfaceChanged(int width, int height) {
        mGLController.serverSurfaceChanged(width, height);

        if (mListener != null) {
            mListener.surfaceChanged(width, height);
        }

        if (mOverscroll != null) {
            mOverscroll.setSize(width, height);
        }
    }

    private void onDestroyed() {
        mGLController.serverSurfaceDestroyed();
    }

    public Object getNativeWindow() {
        if (mSurfaceView != null)
            return mSurfaceView.getHolder();

        return mTextureView.getSurfaceTexture();
    }

    @WrapElementForJNI(allowMultithread = true, stubName = "RegisterCompositorWrapper")
    public static GLController registerCxxCompositor() {
        try {
            LayerView layerView = GeckoAppShell.getLayerView();
            GLController controller = layerView.getGLController();
            controller.compositorCreated();
            return controller;
        } catch (Exception e) {
            Log.e(LOGTAG, "Error registering compositor!", e);
            return null;
        }
    }

    public interface Listener {
        void renderRequested();
        void sizeChanged(int width, int height);
        void surfaceChanged(int width, int height);
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
        LayerView mParent;

        public LayerSurfaceView(Context aContext, LayerView aParent) {
            super(aContext);
            mParent = aParent;
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            if (changed) {
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
    public void addDrawListener(DrawListener listener) {
        mLayerClient.addDrawListener(listener);
    }

    @RobocopTarget
    public void removeDrawListener(DrawListener listener) {
        mLayerClient.removeDrawListener(listener);
    }

    @RobocopTarget
    public static interface DrawListener {
        public void drawFinished();
    }

    @Override
    public void setOverScrollMode(int overscrollMode) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
            super.setOverScrollMode(overscrollMode);
        }
        if (mPanZoomController != null) {
            mPanZoomController.setOverScrollMode(overscrollMode);
        }
    }

    @Override
    public int getOverScrollMode() {
        if (mPanZoomController != null) {
            return mPanZoomController.getOverScrollMode();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
            return super.getOverScrollMode();
        }
        return View.OVER_SCROLL_ALWAYS;
    }

    @Override
    public void onFocusChanged (boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        GeckoAccessibility.onLayerViewFocusChanged(this, gainFocus);
    }

    public void setFullScreenState(FullScreenState state) {
        mFullScreenState = state;
    }

    public boolean isFullScreen() {
        return mFullScreenState != FullScreenState.NONE;
    }

    public FullScreenState getFullScreenState() {
        return mFullScreenState;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        if (msg == Tabs.TabEvents.VIEWPORT_CHANGE && Tabs.getInstance().isSelectedTab(tab) && mLayerClient != null) {
            setZoomConstraints(tab.getZoomConstraints());
            setIsRTL(tab.getIsRTL());
        }
    }

    // Public hooks for listening to metrics changing

    public interface OnMetricsChangedListener {
        public void onMetricsChanged(ImmutableViewportMetrics viewport);
        public void onPanZoomStopped();
    }

    public void setOnMetricsChangedListener(OnMetricsChangedListener listener) {
        mLayerClient.setOnMetricsChangedListener(listener);
    }
}
