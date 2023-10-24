/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import static org.mozilla.geckoview.BuildConfig.DEBUG_BUILD;

import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class GeckoSurface implements Parcelable {
  private static final String LOGTAG = "GeckoSurface";

  private Surface mSurface;
  private long mHandle;
  private boolean mIsSingleBuffer;
  private volatile boolean mIsAvailable;
  private boolean mOwned = true;
  private volatile boolean mIsReleased = false;

  private int mMyPid;
  // Locally allocated surface/texture. Do not pass it over IPC.
  private GeckoSurface mSyncSurface;

  @WrapForJNI(exceptionMode = "nsresult")
  public GeckoSurface(final GeckoSurfaceTexture gst) {
    mSurface = new Surface(gst);
    mHandle = gst.getHandle();
    mIsSingleBuffer = gst.isSingleBuffer();
    mIsAvailable = true;
    mMyPid = android.os.Process.myPid();
  }

  public GeckoSurface(final Parcel p) {
    mSurface = Surface.CREATOR.createFromParcel(p);
    mHandle = p.readLong();
    mIsSingleBuffer = p.readByte() == 1;
    mIsAvailable = p.readByte() == 1;
    mMyPid = p.readInt();
  }

  public static final Parcelable.Creator<GeckoSurface> CREATOR =
      new Parcelable.Creator<GeckoSurface>() {
        public GeckoSurface createFromParcel(final Parcel p) {
          return new GeckoSurface(p);
        }

        public GeckoSurface[] newArray(final int size) {
          return new GeckoSurface[size];
        }
      };

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(final Parcel out, final int flags) {
    mSurface.writeToParcel(out, flags);
    if ((flags & Parcelable.PARCELABLE_WRITE_RETURN_VALUE) == 0) {
      // GeckoSurface can be passed across processes as a return value or
      // an argument, and should always tranfers its ownership (move) to
      // the receiver of parcel. On the other hand, Surface is moved only
      // when passed as a return value and releases itself when corresponding
      // write flags is provided. (See Surface.writeToParcel().)
      // The superclass method must be called here to ensure the local instance
      // is truely forgotten.
      mSurface.release();
    }
    mOwned = false;

    out.writeLong(mHandle);
    out.writeByte((byte) (mIsSingleBuffer ? 1 : 0));
    out.writeByte((byte) (mIsAvailable ? 1 : 0));
    out.writeInt(mMyPid);
  }

  public void release() {
    if (mIsReleased) {
      return;
    }
    mIsReleased = true;

    if (mSyncSurface != null) {
      mSyncSurface.release();
      final GeckoSurfaceTexture gst = GeckoSurfaceTexture.lookup(mSyncSurface.getHandle());
      if (gst != null) {
        gst.decrementUse();
      }
      mSyncSurface = null;
    }

    if (mOwned) {
      mSurface.release();
    }
  }

  @WrapForJNI
  public long getHandle() {
    return mHandle;
  }

  @WrapForJNI
  public Surface getSurface() {
    return mSurface;
  }

  @WrapForJNI
  public boolean getAvailable() {
    return mIsAvailable;
  }

  @WrapForJNI
  public boolean isReleased() {
    return mIsReleased;
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
    final GeckoSurfaceTexture texture = GeckoSurfaceTexture.acquire(true, mHandle);
    if (texture != null) {
      texture.setDefaultBufferSize(width, height);
      texture.track(mHandle);
      mSyncSurface = new GeckoSurface(texture);
      return new SyncConfig(mHandle, mSyncSurface, width, height);
    }

    return null;
  }
}
