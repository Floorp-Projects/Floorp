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
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * A view rendered by the layer compositor.
 */
public class LayerView extends FrameLayout {
    private static final String LOGTAG = "GeckoLayerView";

    private FullScreenState mFullScreenState;

    private int mDefaultClearColor = Color.WHITE;
    /* package */ GetPixelsResult mGetPixelsResult;
    private final List<DrawListener> mDrawListeners;

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

    /* package */ void onCompositorReady() {
        mCompositor.setDefaultClearColor(mDefaultClearColor);
        mCompositor.enableLayerUpdateNotifications(!mDrawListeners.isEmpty());
    }

    /* protected */ LayerSession mSession;
    private LayerSession.Compositor mCompositor;

    public LayerView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mFullScreenState = FullScreenState.NONE;

        mDrawListeners = new ArrayList<DrawListener>();
    }

    public LayerView(Context context) {
        this(context, null);
    }

    private static Point getEventRadius(MotionEvent event) {
        return new Point((int)event.getToolMajor() / 2,
                         (int)event.getToolMinor() / 2);
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
    }

    @WrapForJNI(calledFrom = "ui")
    private Object getCompositor() {
        return isCompositorReady() ? mCompositor : null;
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

    public void setFullScreenState(FullScreenState state) {
        mFullScreenState = state;
    }

    public boolean isFullScreen() {
        return mFullScreenState != FullScreenState.NONE;
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
