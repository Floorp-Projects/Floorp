/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.SurfaceTexture;
import android.os.Build;
import androidx.annotation.RequiresApi;
import android.util.Log;
import android.util.LongSparseArray;
import android.util.SparseArray;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.LinkedList;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

/* package */ final class GeckoSurfaceTexture extends SurfaceTexture {
    private static final String LOGTAG = "GeckoSurfaceTexture";
    private static final int MAX_SURFACE_TEXTURES = 200;
    private static volatile int sNextHandle = 1;
    private static final SparseArray<GeckoSurfaceTexture> sSurfaceTextures = new SparseArray<GeckoSurfaceTexture>();


    private static LongSparseArray<LinkedList<GeckoSurfaceTexture>> sUnusedTextures =
        new LongSparseArray<LinkedList<GeckoSurfaceTexture>>();

    private int mHandle;
    private boolean mIsSingleBuffer;

    private long mAttachedContext;
    private int mTexName;

    private GeckoSurfaceTexture.Callbacks mListener;
    private AtomicInteger mUseCount;
    private boolean mFinalized;

    private int mUpstream;
    private NativeGLBlitHelper mBlitter;

    private GeckoSurfaceTexture(final int handle) {
        super(0);
        init(handle, false);
    }

    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    private GeckoSurfaceTexture(final int handle, final boolean singleBufferMode) {
        super(0, singleBufferMode);
        init(handle, singleBufferMode);
    }

    @Override
    protected void finalize() throws Throwable {
        // We only want finalize() to be called once
        if (mFinalized) {
            return;
        }

        mFinalized = true;
        super.finalize();
    }

    private void init(final int handle, final boolean singleBufferMode) {
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
    public synchronized void attachToGLContext(final long context, final int texName) {
        if (context == mAttachedContext && texName == mTexName) {
            return;
        }

        attachToGLContext(texName);

        mAttachedContext = context;
        mTexName = texName;
    }

    @Override
    @WrapForJNI(exceptionMode = "nsresult")
    public synchronized void detachFromGLContext() {
        super.detachFromGLContext();

        mAttachedContext = mTexName = 0;
    }

    @WrapForJNI
    public synchronized boolean isAttachedToGLContext(final long context) {
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
            if (mUpstream != 0) {
                SurfaceAllocator.sync(mUpstream);
            }
            super.updateTexImage();
            if (mListener != null) {
                mListener.onUpdateTexImage();
            }
        } catch (final Exception e) {
            Log.w(LOGTAG, "updateTexImage() failed", e);
        }
    }

    @Override
    public synchronized void release() {
        mUpstream = 0;
        if (mBlitter != null) {
            mBlitter.close();
        }
        try {
            super.release();
            synchronized (sSurfaceTextures) {
                sSurfaceTextures.remove(mHandle);
            }
        } catch (final Exception e) {
            Log.w(LOGTAG, "release() failed", e);
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
        } catch (final Exception e) {
            Log.w(LOGTAG, "releaseTexImage() failed", e);
        }
    }

    public synchronized void setListener(final GeckoSurfaceTexture.Callbacks listener) {
        mListener = listener;
    }

    @WrapForJNI
    public static boolean isSingleBufferSupported() {
        return Build.VERSION.SDK_INT >= 19;
    }

    @WrapForJNI
    public synchronized void incrementUse() {
        mUseCount.incrementAndGet();
    }

    @WrapForJNI
    public synchronized void decrementUse() {
        final int useCount = mUseCount.decrementAndGet();

        if (useCount == 0) {
            setListener(null);

            if (mAttachedContext == 0) {
                release();
                synchronized (sUnusedTextures) {
                    sSurfaceTextures.remove(mHandle);
                }
                return;
            }

            synchronized (sUnusedTextures) {
                LinkedList<GeckoSurfaceTexture> list = sUnusedTextures.get(mAttachedContext);
                if (list == null) {
                    list = new LinkedList<GeckoSurfaceTexture>();
                    sUnusedTextures.put(mAttachedContext, list);
                }
                list.addFirst(this);
            }
        }
    }

    @WrapForJNI
    public static void destroyUnused(final long context) {
        final LinkedList<GeckoSurfaceTexture> list;
        synchronized (sUnusedTextures) {
            list = sUnusedTextures.get(context);
            sUnusedTextures.delete(context);
        }

        if (list == null) {
            return;
        }

        for (final GeckoSurfaceTexture tex : list) {
            try {
                if (tex.isSingleBuffer()) {
                    tex.releaseTexImage();
                }

                tex.detachFromGLContext();
                tex.release();

                // We need to manually call finalize here, otherwise we can run out
                // of file descriptors if the GC doesn't kick in soon enough. Bug 1416015.
                try {
                    tex.finalize();
                } catch (final Throwable t) {
                    Log.e(LOGTAG, "Failed to finalize SurfaceTexture", t);
                }
            } catch (final Exception e) {
                Log.e(LOGTAG, "Failed to destroy SurfaceTexture", e);
            }
        }
    }

    public static GeckoSurfaceTexture acquire(final boolean singleBufferMode, final int handle) {
        if (singleBufferMode && !isSingleBufferSupported()) {
            throw new IllegalArgumentException("single buffer mode not supported on API version < 19");
        }

        synchronized (sSurfaceTextures) {
            // We want to limit the maximum number of SurfaceTextures at any one time.
            // This is because they use a large number of fds, and once the process' limit
            // is reached bad things happen. See bug 1421586.
            if (sSurfaceTextures.size() >= MAX_SURFACE_TEXTURES) {
                return null;
            }

            int resolvedHandle = handle;
            if (resolvedHandle == 0) {
                // Generate new handle value when none specified.
                resolvedHandle = sNextHandle++;
            }

            final GeckoSurfaceTexture gst;
            if (isSingleBufferSupported()) {
                gst = new GeckoSurfaceTexture(resolvedHandle, singleBufferMode);
            } else {
                gst = new GeckoSurfaceTexture(resolvedHandle);
            }

            if (sSurfaceTextures.indexOfKey(resolvedHandle) >= 0) {
                gst.release();
                throw new IllegalArgumentException("Already have a GeckoSurfaceTexture with that handle");
            }

            sSurfaceTextures.put(resolvedHandle, gst);
            return gst;
        }
    }

    @WrapForJNI
    public static GeckoSurfaceTexture lookup(final int handle) {
        synchronized (sSurfaceTextures) {
            return sSurfaceTextures.get(handle);
        }
    }

    /* package */ synchronized void track(final int upstream) {
        mUpstream = upstream;
    }

    /* package */ synchronized void configureSnapshot(final GeckoSurface target,
                                                      final int width, final int height) {
        mBlitter = NativeGLBlitHelper.create(mHandle, target, width, height);
    }

    /* package */ synchronized void takeSnapshot() {
        mBlitter.blit();
    }

    public interface Callbacks {
        void onUpdateTexImage();
        void onReleaseTexImage();
    }

    @WrapForJNI
    public static final class NativeGLBlitHelper extends JNIObject {
        public static NativeGLBlitHelper create(final int textureHandle,
                                                final GeckoSurface targetSurface,
                                                final int width,
                                                final int height) {
            final NativeGLBlitHelper helper = nativeCreate(textureHandle, targetSurface, width, height);
            helper.mTargetSurface = targetSurface; // Take ownership of surface.
            return helper;
        }

        public native static NativeGLBlitHelper nativeCreate(final int textureHandle,
                                                       final GeckoSurface targetSurface,
                                                       final int width,
                                                       final int height);
        public native void blit();

        public void close() {
            disposeNative();
            if (mTargetSurface != null) {
                mTargetSurface.release();
                mTargetSurface = null;
            }
        }

        @Override
        protected native void disposeNative();

        private GeckoSurface mTargetSurface;
    }
}
