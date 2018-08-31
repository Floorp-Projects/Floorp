/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.media;

import android.os.Parcel;
import android.os.Parcelable;

public final class MediaNotification implements Parcelable {
    private final int visibility;
    private final int tabId;
    private final String title;
    private final String text;
    private final byte[] bitmapBytes;

    /* package */ MediaNotification(int visibility, int tabId,
                      String title, String content, byte[] bitmapByteArray) {
        this.visibility = visibility;
        this.tabId = tabId;
        this.title = title;
        this.text = content;
        this.bitmapBytes = bitmapByteArray;
    }

    /* package */ int getVisibility() {
        return visibility;
    }

    /* package */ int getTabId() {
        return tabId;
    }

    /* package */ String getTitle() {
        return title;
    }

    /* package */ String getText() {
        return text;
    }

    /* package */ byte[] getBitmapBytes() {
        return bitmapBytes;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(visibility);
        dest.writeInt(tabId);
        dest.writeString(title);
        dest.writeString(text);
        dest.writeInt(bitmapBytes.length);
        dest.writeByteArray(bitmapBytes);
    }

    public static final Parcelable.Creator<MediaNotification> CREATOR = new Parcelable.Creator<MediaNotification>() {
        @Override
        public MediaNotification createFromParcel(final Parcel source) {
            final int visibility = source.readInt();
            final int tabId = source.readInt();
            final String title = source.readString();
            final String text = source.readString();
            final int arrayLength = source.readInt();
            final byte[] bitmapBytes = new byte[arrayLength];
            source.readByteArray(bitmapBytes);

            return new MediaNotification(visibility, tabId, title, text, bitmapBytes);
        }

        @Override
        public MediaNotification[] newArray(int size) {
            return new MediaNotification[size];
        }
    };
}
