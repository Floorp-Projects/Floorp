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
    private int mTexName;
    private GeckoSurfaceTexture.Callbacks mListener;
    private AtomicInteger mUseCount;

    @WrapForJNI(dispatchTo = "current")
    private static native int nativeAcquireTexture();

    private GeckoSurfaceTexture(int handle, int texName) {
        super(texName);
        init(handle, texName, false);
    }

    private GeckoSurfaceTexture(int handle, int texName, boolean singleBufferMode) {
        super(texName, singleBufferMode);
        init(handle, texName, singleBufferMode);
    }

    private void init(int handle, int texName, boolean singleBufferMode) {
        mHandle = handle;
        mIsSingleBuffer = singleBufferMode;
        mTexName = texName;
        mUseCount = new AtomicInteger(1);
    }

    @WrapForJNI
    public int getHandle() {
        return mHandle;
    }

    @WrapForJNI
    public int getTexName() {
        return mTexName;
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

        super.releaseTexImage();
        if (mListener != null) {
            mListener.onReleaseTexImage();
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
        int texName = nativeAcquireTexture();

        final GeckoSurfaceTexture gst;
        if (isSingleBufferSupported()) {
            gst = new GeckoSurfaceTexture(handle, texName, singleBufferMode);
        } else {
            gst = new GeckoSurfaceTexture(handle, texName);
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