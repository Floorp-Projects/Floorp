/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.SurfaceTexture;
import android.util.Log;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.HashMap;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants.Versions;

public final class GeckoSurfaceTexture extends SurfaceTexture {
    private static final String LOGTAG = "GeckoSurfaceTexture";
    private static volatile int sNextHandle = 1;
    private static HashMap<Integer, GeckoSurfaceTexture> sSurfaceTextures = new HashMap<Integer, GeckoSurfaceTexture>();

    private int mHandle;
    private boolean mIsSingleBuffer;

    private long mAttachedContext;
    private int mTexName;

    private GeckoSurfaceTexture.Callbacks mListener;
    private AtomicInteger mUseCount;

    private GeckoSurfaceTexture(int handle) {
        super(0);
        init(handle, false);
    }

    private GeckoSurfaceTexture(int handle, boolean singleBufferMode) {
        super(0, singleBufferMode);
        init(handle, singleBufferMode);
    }

    private void init(int handle, boolean singleBufferMode) {
        mHandle = handle;
        mIsSingleBuffer = singleBufferMode;
        mUseCount = new AtomicInteger(1);

        // Start off detached
        detachFromGLContext();
    }

    @WrapForJNI
    public int getHandle() {
        return mHandle;
    }

    @WrapForJNI
    public int getTexName() {
        return mTexName;
    }

    @WrapForJNI(exceptionMode = "nsresult")
    public void attachToGLContext(long context, int texName) {
        if (context == mAttachedContext && texName == mTexName) {
            return;
        }

        attachToGLContext(texName);

        mAttachedContext = context;
        mTexName = texName;
    }

    @Override
    @WrapForJNI(exceptionMode = "nsresult")
    public void detachFromGLContext() {
        super.detachFromGLContext();

        mAttachedContext = mTexName = 0;
    }

    @WrapForJNI
    public boolean isAttachedToGLContext(long context) {
        return mAttachedContext == context;
    }

    @WrapForJNI
    public boolean isSingleBuffer() {
        return mIsSingleBuffer;
    }

    @Override
    @WrapForJNI
    public synchronized void updateTexImage() {
        try {
            super.updateTexImage();
            if (mListener != null) {
                mListener.onUpdateTexImage();
            }
        } catch (Exception e) {
            Log.w(LOGTAG, "updateTexImage() failed", e);
        }
    }

    @Override
    @WrapForJNI
    public synchronized void releaseTexImage() {
        if (!mIsSingleBuffer) {
            return;
        }

        try {
            super.releaseTexImage();
            if (mListener != null) {
                mListener.onReleaseTexImage();
            }
        } catch (Exception e) {
            Log.w(LOGTAG, "releaseTexImage() failed", e);
        }
    }

    public synchronized void setListener(GeckoSurfaceTexture.Callbacks listener) {
        mListener = listener;
    }

    @WrapForJNI
    public static boolean isSingleBufferSupported() {
        return Versions.feature19Plus;
    }

    @WrapForJNI
    public void incrementUse() {
        mUseCount.incrementAndGet();
    }

    @WrapForJNI
    public void decrementUse() {
        int useCount = mUseCount.decrementAndGet();

        if (useCount == 0) {
            synchronized (sSurfaceTextures) {
                sSurfaceTextures.remove(mHandle);
            }

            setListener(null);

            if (Versions.feature16Plus) {
                try {
                    detachFromGLContext();
                } catch (Exception e) {
                    // This can throw if the EGL context is not current
                    // but we can't do anything about that now.
                }
            }

            release();
        }
    }

    public static GeckoSurfaceTexture acquire(boolean singleBufferMode) {
        if (singleBufferMode && !isSingleBufferSupported()) {
            throw new IllegalArgumentException("single buffer mode not supported on API version < 19");
        }

        int handle = sNextHandle++;

        final GeckoSurfaceTexture gst;
        if (isSingleBufferSupported()) {
            gst = new GeckoSurfaceTexture(handle, singleBufferMode);
        } else {
            gst = new GeckoSurfaceTexture(handle);
        }

        synchronized (sSurfaceTextures) {
            if (sSurfaceTextures.containsKey(handle)) {
                gst.release();
                throw new IllegalArgumentException("Already have a GeckoSurfaceTexture with that handle");
            }

            sSurfaceTextures.put(handle, gst);
        }


        return gst;
    }

    @WrapForJNI
    public static GeckoSurfaceTexture lookup(int handle) {
        synchronized (sSurfaceTextures) {
            return sSurfaceTextures.get(handle);
        }
    }

    public interface Callbacks {
        void onUpdateTexImage();
        void onReleaseTexImage();
    }
}