/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.ArrayList;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * A thin representation of a remote client.
 * <p>
 * We use the hash of the client's GUID as the ID elsewhere.
 */
public class RemoteClient implements Parcelable {
    public final String guid;
    public final String name;
    public final long lastModified;
    public final String deviceType;
    public final ArrayList<RemoteTab> tabs;

    public RemoteClient(String guid, String name, long lastModified, String deviceType) {
        this.guid = guid;
        this.name = name;
        this.lastModified = lastModified;
        this.deviceType = deviceType;
        this.tabs = new ArrayList<>();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeString(guid);
        parcel.writeString(name);
        parcel.writeLong(lastModified);
        parcel.writeString(deviceType);
        parcel.writeTypedList(tabs);
    }

    public static final Creator<RemoteClient> CREATOR = new Creator<RemoteClient>() {
        @Override
        public RemoteClient createFromParcel(final Parcel source) {
            final String guid = source.readString();
            final String name = source.readString();
            final long lastModified = source.readLong();
            final String deviceType = source.readString();

            final RemoteClient client = new RemoteClient(guid, name, lastModified, deviceType);
            source.readTypedList(client.tabs, RemoteTab.CREATOR);

            return client;
        }

        @Override
        public RemoteClient[] newArray(final int size) {
            return new RemoteClient[size];
        }
    };

    public boolean isDesktop() {
        return "desktop".equals(deviceType);
    }
}
