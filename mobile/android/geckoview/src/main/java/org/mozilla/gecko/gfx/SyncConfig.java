/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.os.Parcel;
import android.os.Parcelable;

/* package */ final class SyncConfig implements Parcelable {
  final long sourceTextureHandle;
  final GeckoSurface targetSurface;
  final int width;
  final int height;

  /* package */ SyncConfig(
      final long sourceTextureHandle,
      final GeckoSurface targetSurface,
      final int width,
      final int height) {
    this.sourceTextureHandle = sourceTextureHandle;
    this.targetSurface = targetSurface;
    this.width = width;
    this.height = height;
  }

  public static final Creator<SyncConfig> CREATOR =
      new Creator<SyncConfig>() {
        @Override
        public SyncConfig createFromParcel(final Parcel parcel) {
          return new SyncConfig(parcel);
        }

        @Override
        public SyncConfig[] newArray(final int i) {
          return new SyncConfig[i];
        }
      };

  private SyncConfig(final Parcel parcel) {
    sourceTextureHandle = parcel.readLong();
    targetSurface = GeckoSurface.CREATOR.createFromParcel(parcel);
    width = parcel.readInt();
    height = parcel.readInt();
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(final Parcel parcel, final int flags) {
    parcel.writeLong(sourceTextureHandle);
    targetSurface.writeToParcel(parcel, flags);
    parcel.writeInt(width);
    parcel.writeInt(height);
  }
}
