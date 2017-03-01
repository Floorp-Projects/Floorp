/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.os.Parcel;
import android.os.Parcelable;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class SessionKeyInfo implements Parcelable {
    @WrapForJNI
    public byte[] keyId;

    @WrapForJNI
    public int status;

    @WrapForJNI
    public SessionKeyInfo(byte[] keyId, int status) {
        this.keyId = keyId;
        this.status = status;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int parcelableFlags) {
        dest.writeByteArray(keyId);
        dest.writeInt(status);
    }

    public static final Creator<SessionKeyInfo> CREATOR = new Creator<SessionKeyInfo>() {
        @Override
        public SessionKeyInfo createFromParcel(Parcel in) {
            return new SessionKeyInfo(in);
        }

        @Override
        public SessionKeyInfo[] newArray(int size) {
            return new SessionKeyInfo[size];
        }
    };

    private SessionKeyInfo(Parcel src) {
        keyId = src.createByteArray();
        status = src.readInt();
    }
}
