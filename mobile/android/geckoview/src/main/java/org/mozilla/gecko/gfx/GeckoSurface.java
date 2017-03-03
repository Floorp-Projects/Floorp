/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.SurfaceTexture;

import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;
import android.util.Log;

import java.util.HashMap;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants.Versions;

public final class GeckoSurface extends Surface {
    private static final String LOGTAG = "GeckoSurface";

    private static HashMap<Integer, GeckoSurfaceTexture> sSurfaceTextures = new HashMap<Integer, GeckoSurfaceTexture>();

    private int mHandle;
    private boolean mIsSingleBuffer;
    private volatile boolean mIsAvailable;

    private SurfaceTexture mDummySurfaceTexture;

    @WrapForJNI(exceptionMode = "nsresult")
    public GeckoSurface(GeckoSurfaceTexture gst) {
        super(gst);
        mHandle = gst.getHandle();
        mIsSingleBuffer = gst.isSingleBuffer();
        mIsAvailable = true;
    }

    public GeckoSurface(SurfaceTexture st) {
        super(st);
        mDummySurfaceTexture = st;
    }

    public GeckoSurface() {
        // A no-arg constructor exists, but is hidden in the SDK. We need to create a dummy
        // SurfaceTexture here in order to create the instance. This is used to transfer the
        // GeckoSurface across binder.
        super(new SurfaceTexture(0));
    }

    @Override
    public void readFromParcel(Parcel p) {
        super.readFromParcel(p);
        mHandle = p.readInt();
        mIsSingleBuffer = p.readByte() == 1 ? true : false;
        mIsAvailable = (p.readByte() == 1 ? true : false);

        if (mDummySurfaceTexture != null) {
            mDummySurfaceTexture.release();
            mDummySurfaceTexture = null;
        }
    }

    public static final Parcelable.Creator<GeckoSurface> CREATOR = new Parcelable.Creator<GeckoSurface>() {
        public GeckoSurface createFromParcel(Parcel p) {
            GeckoSurface surf = new GeckoSurface();
            surf.readFromParcel(p);
            return surf;
        }

        public GeckoSurface[] newArray(int size) {
            return new GeckoSurface[size];
        }
    };

    @Override
    public void writeToParcel(Parcel out, int flags) {
        super.writeToParcel(out, flags);
        out.writeInt(mHandle);
        out.writeByte((byte) (mIsSingleBuffer ? 1 : 0));
        out.writeByte((byte) (mIsAvailable ? 1 : 0));
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
    public void setAvailable(boolean available) {
        mIsAvailable = available;
    }
}