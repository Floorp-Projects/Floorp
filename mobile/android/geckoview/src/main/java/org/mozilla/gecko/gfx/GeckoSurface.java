/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.SurfaceTexture;

import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;

import org.mozilla.gecko.annotation.WrapForJNI;

import static org.mozilla.geckoview.BuildConfig.DEBUG_BUILD;

public final class GeckoSurface extends Surface {
    private static final String LOGTAG = "GeckoSurface";

    private int mHandle;
    private boolean mIsSingleBuffer;
    private volatile boolean mIsAvailable;
    private boolean mOwned = true;

    private int mMyPid;
    // Locally allocated surface/texture. Do not pass it over IPC.
    private GeckoSurface mSyncSurface;

    @WrapForJNI(exceptionMode = "nsresult")
    public GeckoSurface(final GeckoSurfaceTexture gst) {
        super(gst);
        mHandle = gst.getHandle();
        mIsSingleBuffer = gst.isSingleBuffer();
        mIsAvailable = true;
        mMyPid = android.os.Process.myPid();
    }

    public GeckoSurface(final Parcel p, final SurfaceTexture dummy) {
        // A no-arg constructor exists, but is hidden in the SDK. We need to create a dummy
        // SurfaceTexture here in order to create the instance. This is used to transfer the
        // GeckoSurface across binder.
        super(dummy);

        readFromParcel(p);
        mHandle = p.readInt();
        mIsSingleBuffer = p.readByte() == 1 ? true : false;
        mIsAvailable = (p.readByte() == 1 ? true : false);
        mMyPid = p.readInt();

        dummy.release();
    }

    public static final Parcelable.Creator<GeckoSurface> CREATOR = new Parcelable.Creator<GeckoSurface>() {
        public GeckoSurface createFromParcel(final Parcel p) {
            return new GeckoSurface(p, new SurfaceTexture(0));
        }

        public GeckoSurface[] newArray(final int size) {
            return new GeckoSurface[size];
        }
    };

    @Override
    public void writeToParcel(final Parcel out, final int flags) {
        super.writeToParcel(out, flags);
        out.writeInt(mHandle);
        out.writeByte((byte) (mIsSingleBuffer ? 1 : 0));
        out.writeByte((byte) (mIsAvailable ? 1 : 0));
        out.writeInt(mMyPid);

        mOwned = false;
    }

    @Override
    public void release() {
        if (mSyncSurface != null) {
            mSyncSurface.release();
            GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(mSyncSurface.getHandle());
            if (gst != null) {
                gst.decrementUse();
            }
            mSyncSurface = null;
        }

        if (mOwned) {
            super.release();
        }
    }

    @WrapForJNI
    public int getHandle() {
        return mHandle;
    }

    @WrapForJNI
    public boolean getAvailable() {
        return mIsAvailable;
    }

    @WrapForJNI
    public void setAvailable(final boolean available) {
        mIsAvailable = available;
    }

    /* package */ boolean inProcess() {
        return android.os.Process.myPid() == mMyPid;
    }

    /* package */ SyncConfig initSyncSurface(final int width, final int height) {
        if (DEBUG_BUILD) {
            if (inProcess()) {
                throw new AssertionError("no need for sync when allocated in process");
            }
        }
        if (GeckoSurfaceTexture.lookup(mHandle) != null) {
            throw new AssertionError("texture#" + mHandle + " already in use.");
        }
        GeckoSurfaceTexture texture = GeckoSurfaceTexture.acquire(false, mHandle);
        texture.setDefaultBufferSize(width, height);
        texture.track(mHandle);
        mSyncSurface = new GeckoSurface(texture);

        return new SyncConfig(mHandle, mSyncSurface, width, height);
    }
}
