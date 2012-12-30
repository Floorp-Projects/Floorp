/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.GeckoAccessibility;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.util.AttributeSet;
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

import java.lang.reflect.Method;

/**
 * A view rendered by the layer compositor.
 *
 * Note that LayerView is accessed by Robocop via reflection.
 */
public class LayerView extends FrameLayout {
    private static String LOGTAG = "GeckoLayerView";

    private GeckoLayerClient mLayerClient;
    private TouchEventHandler mTouchEventHandler;
    private GLController mGLController;
    private InputConnectionHandler mInputConnectionHandler;
    private LayerRenderer mRenderer;
    /* Must be a PAINT_xxx constant */
    private int mPaintState;
    private int mBackgroundColor;
    private boolean mFullScreen;

    private SurfaceView mSurfaceView;
    private TextureView mTextureView;

    private Listener mListener;

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

        mGLController = new GLController(this);
        mPaintState = PAINT_START;
        mBackgroundColor = Color.WHITE;
    }

    public void initializeView(EventDispatcher eventDispatcher) {
        mLayerClient = new GeckoLayerClient(getContext(), this, eventDispatcher);

        mTouchEventHandler = new TouchEventHandler(getContext(), this, mLayerClient);
        mRenderer = new LayerRenderer(this);
        mInputConnectionHandler = null;

        setFocusable(true);
        setFocusableInTouchMode(true);

        GeckoAccessibility.setDelegate(this);
    }

    public void show() {
        // Fix this if TextureView support is turned back on above
        mSurfaceView.setVisibility(View.VISIBLE);
    }

    public void hide() {
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
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
            requestFocus();

        /** We need to manually hide FormAssistPopup because it is not a regular PopupWindow. */
        if (GeckoApp.mAppContext != null)
            GeckoApp.mAppContext.hideFormAssistPopup();

        return mTouchEventHandler == null ? false : mTouchEventHandler.handleEvent(event);
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        return mTouchEventHandler == null ? false : mTouchEventHandler.handleEvent(event);
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
            mTextureView.setBackgroundColor(Color.WHITE);
            addView(mTextureView, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        } else {
            // This will stop PropertyAnimator from creating a drawing cache (i.e. a bitmap)
            // from a SurfaceView, which is just not possible (the bitmap will be transparent).
            setWillNotCacheDrawing(false);

            mSurfaceView = new SurfaceView(getContext());
            mSurfaceView.setBackgroundColor(Color.WHITE);
            addView(mSurfaceView, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

            SurfaceHolder holder = mSurfaceView.getHolder();
            holder.addCallback(new SurfaceListener());
            holder.setFormat(PixelFormat.RGB_565);
        }
    }

    public GeckoLayerClient getLayerClient() { return mLayerClient; }
    public TouchEventHandler getTouchEventHandler() { return mTouchEventHandler; }

    public ImmutableViewportMetrics getViewportMetrics() {
        return mLayerClient.getViewportMetrics();
    }

    public void abortPanning() {
        mLayerClient.getPanZoomController().abortPanning();
    }

    public PointF convertViewPointToLayerPoint(PointF viewPoint) {
        return mLayerClient.convertViewPointToLayerPoint(viewPoint);
    }

    int getBackgroundColor() {
        return mBackgroundColor;
    }

    public void setBackgroundColor(int newColor) {
        mBackgroundColor = newColor;
        requestRender();
    }

    public void setZoomConstraints(ZoomConstraints constraints) {
        mLayerClient.setZoomConstraints(constraints);
    }

    public void setViewportSize(int width, int height) {
        mLayerClient.setViewportSize(width, height);
    }

    public void setInputConnectionHandler(InputConnectionHandler inputConnectionHandler) {
        mInputConnectionHandler = inputConnectionHandler;
        mLayerClient.setForceRedraw();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onCreateInputConnection(outAttrs);
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyPreIme(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyDown(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyLongPress(keyCode, event);
        return false;
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyMultiple(keyCode, repeatCount, event);
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mInputConnectionHandler != null)
            return mInputConnectionHandler.onKeyUp(keyCode, event);
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

    public int getMaxTextureSize() {
        return mRenderer.getMaxTextureSize();
    }

    /** Used by robocop for testing purposes. Not for production use! */
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

    private Bitmap getDrawable(int resId) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inScaled = false;
        return BitmapFactory.decodeResource(getContext().getResources(), resId, options);
    }

    Bitmap getBackgroundPattern() {
        return getDrawable(R.drawable.abouthome_bg);
    }

    Bitmap getShadowPattern() {
        return getDrawable(R.drawable.shadow);
    }

    Bitmap getScrollbarImage() {
        return getDrawable(R.drawable.scrollbar);
    }

    private void onSizeChanged(int width, int height) {
        mGLController.surfaceChanged(width, height);

        if (mListener != null) {
            mListener.surfaceChanged(width, height);
        }
    }

    private void onDestroyed() {
        mGLController.surfaceDestroyed();

        if (mListener != null) {
            mListener.compositionPauseRequested();
        }
    }

    public Object getNativeWindow() {
        if (mSurfaceView != null)
            return mSurfaceView.getHolder();

        return mTextureView.getSurfaceTexture();
    }

    /** This function is invoked by Gecko (compositor thread) via JNI; be careful when modifying signature. */
    public static GLController registerCxxCompositor() {
        try {
            LayerView layerView = GeckoApp.mAppContext.getLayerView();
            layerView.mListener.compositorCreated();
            return layerView.getGLController();
        } catch (Exception e) {
            Log.e(LOGTAG, "Error registering compositor!", e);
            return null;
        }
    }

    public interface Listener {
        void compositorCreated();
        void renderRequested();
        void compositionPauseRequested();
        void compositionResumeRequested(int width, int height);
        void surfaceChanged(int width, int height);
    }

    private class SurfaceListener implements SurfaceHolder.Callback {
        public void surfaceChanged(SurfaceHolder holder, int format, int width,
                                                int height) {
            onSizeChanged(width, height);
        }

        public void surfaceCreated(SurfaceHolder holder) {
        }

        public void surfaceDestroyed(SurfaceHolder holder) {
            onDestroyed();
        }
    }

    private class SurfaceTextureListener implements TextureView.SurfaceTextureListener {
        public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
            // We don't do this for surfaceCreated above because it is always followed by a surfaceChanged,
            // but that is not the case here.
            onSizeChanged(width, height);
        }

        public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
            onDestroyed();
            return true; // allow Android to call release() on the SurfaceTexture, we are done drawing to it
        }

        public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
            onSizeChanged(width, height);
        }

        public void onSurfaceTextureUpdated(SurfaceTexture surface) {

        }
    }

    @Override
    public void setOverScrollMode(int overscrollMode) {
        super.setOverScrollMode(overscrollMode);
        if (mLayerClient != null)
            mLayerClient.getPanZoomController().setOverScrollMode(overscrollMode);
    }

    @Override
    public int getOverScrollMode() {
        if (mLayerClient != null)
            return mLayerClient.getPanZoomController().getOverScrollMode();
        return super.getOverScrollMode();
    }

    @Override
    public void onFocusChanged (boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);
        GeckoAccessibility.onLayerViewFocusChanged(this, gainFocus);
    }

    public void setFullScreen(boolean fullScreen) {
        mFullScreen = fullScreen;
    }

    public boolean isFullScreen() {
        return mFullScreen;
    }
}
