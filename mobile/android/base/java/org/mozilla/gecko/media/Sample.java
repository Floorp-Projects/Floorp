/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.os.Parcel;
import android.os.Parcelable;

import java.nio.ByteBuffer;

// POD carrying input sample data and info cross process.
public final class Sample implements Parcelable {
    public static final Sample EOS;
    static {
        BufferInfo eosInfo = new BufferInfo();
        eosInfo.set(0, 0, Long.MIN_VALUE, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
        EOS = new Sample(null, eosInfo);
    }

    public BufferInfo info;
    public ByteBuffer bytes;

    public Sample(ByteBuffer bytes, BufferInfo info) {
        this.info = info;
        this.bytes = bytes;
    }

    protected Sample(Parcel in) {
        readFromParcel(in);
    }

    public static Sample createDummyWithInfo(BufferInfo info) {
        BufferInfo dummyInfo = new BufferInfo();
        dummyInfo.set(0, 0, info.presentationTimeUs, info.flags);
        return new Sample(null, dummyInfo);
    }

    public boolean isDummy() {
        return bytes == null && info.size == 0;
    }

    public boolean isEOS() {
        return (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
    }

    public static final Creator<Sample> CREATOR = new Creator<Sample>() {
        @Override
        public Sample createFromParcel(Parcel in) {
            return new Sample(in);
        }

        @Override
        public Sample[] newArray(int size) {
            return new Sample[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    public void readFromParcel(Parcel in) {
        long pts = in.readLong();
        int flags = in.readInt();
        int size = 0;
        byte[] buf = in.createByteArray();
        if (buf != null) {
            bytes = ByteBuffer.wrap(buf);
            size = buf.length;
        }
        info = new BufferInfo();
        info.set(0, size, pts, flags);
    }

    @Override
    public void writeToParcel(Parcel dest, int parcelableFlags) {
        dest.writeLong(info.presentationTimeUs);
        dest.writeInt(info.flags);
        dest.writeByteArray(byteArrayFromBuffer(bytes, info.offset, info.size));
    }

    public static byte[] byteArrayFromBuffer(ByteBuffer buffer, int offset, int size) {
        if (buffer == null || buffer.capacity() == 0 || size == 0) {
            return null;
        }
        if (buffer.hasArray() && offset == 0 && buffer.array().length == size) {
            return buffer.array();
        }
        int length = Math.min(offset + size, buffer.capacity()) - offset;
        byte[] bytes = new byte[length];
        buffer.position(offset);
        buffer.get(bytes);
        return bytes;
    }

    public byte[] getBytes() {
        return byteArrayFromBuffer(bytes, info.offset, info.size);
    }

    @Override
    public String toString() {
        if (isEOS()) {
            return "EOS sample";
        } else {
            StringBuilder str = new StringBuilder();
            str.append("{ pts=").append(info.presentationTimeUs);
            if (bytes != null) {
                str.append(", size=").append(info.size);
            }
            str.append(", flags=").append(Integer.toHexString(info.flags)).append(" }");
            return str.toString();
        }
    }
}