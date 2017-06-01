/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.SurfaceTexture;
import android.util.Log;

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
    private volatile int mUseCount = 1;

    @WrapForJNI(dispatchTo = "current")
    private static native int nativeAcquireTexture();

    private GeckoSurfaceTexture(int handle, int texName) {
        super(texName);
        mHandle = handle;
        mIsSingleBuffer = false;
        mTexName = texName;
    }

    private GeckoSurfaceTexture(int handle, int texName, boolean singleBufferMode) {
        super(texName, singleBufferMode);
        mHandle = handle;
        mIsSingleBuffer = singleBufferMode;
        mTexName = texName;
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
        super.updateTexImage();
        if (mListener != null) {
            mListener.onUpdateTexImage();
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
        mUseCount++;
    }

    @WrapForJNI
    public void decrementUse() {
        mUseCount--;

        if (mUseCount == 0) {
            synchronized (sSurfaceTextures) {
                sSurfaceTextures.remove(mHandle);
            }

            setListener(null);

            if (Versions.feature16Plus) {
                detachFromGLContext();
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