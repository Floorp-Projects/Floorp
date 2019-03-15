package org.mozilla.gecko.gfx;

import android.os.Parcel;
import android.os.Parcelable;

/* package */ final class SyncConfig implements Parcelable {
    final int sourceTextureHandle;
    final GeckoSurface targetSurface;
    final int width;
    final int height;

    /* package */ SyncConfig(int sourceTextureHandle,
                             GeckoSurface targetSurface,
                             int width,
                             int height) {
        this.sourceTextureHandle = sourceTextureHandle;
        this.targetSurface = targetSurface;
        this.width = width;
        this.height = height;
    }

    public static final Creator<SyncConfig> CREATOR =
        new Creator<SyncConfig>() {
            @Override
            public SyncConfig createFromParcel(Parcel parcel) {
                return new SyncConfig(parcel);
            }

            @Override
            public SyncConfig[] newArray(int i) {
                return new SyncConfig[i];
            }
        };

    private SyncConfig(Parcel parcel) {
        sourceTextureHandle = parcel.readInt();
        targetSurface = GeckoSurface.CREATOR.createFromParcel(parcel);
        width = parcel.readInt();
        height = parcel.readInt();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeInt(sourceTextureHandle);
        targetSurface.writeToParcel(parcel, flags);
        parcel.writeInt(width);
        parcel.writeInt(height);
    }
}
