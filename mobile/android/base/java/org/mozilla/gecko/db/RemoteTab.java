/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * A thin representation of a remote tab.
 * <p>
 * These are generated functions.
 */
public class RemoteTab implements Parcelable {
    public final String title;
    public final String url;
    public final long lastUsed;

    public RemoteTab(String title, String url, long lastUsed) {
        this.title = title;
        this.url = url;
        this.lastUsed = lastUsed;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeString(title);
        parcel.writeString(url);
        parcel.writeLong(lastUsed);
    }

    public static final Creator<RemoteTab> CREATOR = new Creator<RemoteTab>() {
        @Override
        public RemoteTab createFromParcel(final Parcel source) {
            final String title = source.readString();
            final String url = source.readString();
            final long lastUsed = source.readLong();
            return new RemoteTab(title, url, lastUsed);
        }

        @Override
        public RemoteTab[] newArray(final int size) {
            return new RemoteTab[size];
        }
    };

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((title == null) ? 0 : title.hashCode());
        result = prime * result + ((url == null) ? 0 : url.hashCode());
        return result;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (getClass() != obj.getClass()) {
            return false;
        }
        RemoteTab other = (RemoteTab) obj;
        if (title == null) {
            if (other.title != null) {
                return false;
            }
        } else if (!title.equals(other.title)) {
            return false;
        }
        if (url == null) {
            if (other.url != null) {
                return false;
            }
        } else if (!url.equals(other.url)) {
            return false;
        }
        return true;
    }
}
