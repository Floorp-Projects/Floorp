/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.service.sharemethods;

import android.os.Parcel;
import android.os.Parcelable;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

/**
 * An immutable representation of a Sync ClientRecord for Parceling, storing only name, guid, type.
 * Implemented this way instead of by making ClientRecord itself parcelable to avoid an undesirable
 * dependency between Sync and the IPC system used by the share system (things which really should
 * be kept as independent as possible).
 */
public class ParcelableClientRecord implements Parcelable {
    private static final String LOGTAG = "GeckoParcelableClientRecord";

    public final String name;
    public final String type;
    public final String guid;

    private ParcelableClientRecord(String aName, String aType, String aGUID) {
        name = aName;
        type = aType;
        guid = aGUID;
    }

    /**
     * Create a ParcelableClientRecord from a vanilla ClientRecord.
     */
    public static ParcelableClientRecord fromClientRecord(ClientRecord record) {
        return new ParcelableClientRecord(record.name, record.type, record.guid);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel parcel, int flags) {
        parcel.writeString(name);
        parcel.writeString(type);
        parcel.writeString(guid);
    }

    public static final Creator<ParcelableClientRecord> CREATOR = new Creator<ParcelableClientRecord>() {
        @Override
        public ParcelableClientRecord createFromParcel(final Parcel source) {
            String name = source.readString();
            String type = source.readString();
            String guid = source.readString();

            return new ParcelableClientRecord(name, type, guid);
        }

        @Override
        public ParcelableClientRecord[] newArray(final int size) {
            return new ParcelableClientRecord[size];
        }
    };

    /**
     * Used by SendTabDeviceListArrayAdapter to populate ListViews.
     */
    @Override
    public String toString() {
        return name;
    }
}
