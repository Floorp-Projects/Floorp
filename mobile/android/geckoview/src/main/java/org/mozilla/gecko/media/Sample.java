/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.os.Parcel;
import android.os.Parcelable;

import org.mozilla.gecko.annotation.WrapForJNI;

import java.nio.ByteBuffer;

// Parcelable carrying input/output sample data and info cross process.
public final class Sample implements Parcelable {
    public static final Sample EOS;
    static {
        BufferInfo eosInfo = new BufferInfo();
        EOS = new Sample();
        EOS.info.set(0, 0, Long.MIN_VALUE, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
    }
    @WrapForJNI
    public long session;

    public static final int NO_BUFFER = -1;

    public int bufferId = NO_BUFFER;
    @WrapForJNI
    public BufferInfo info = new BufferInfo();
    public CryptoInfo cryptoInfo;

    // Simple Linked list for recycling objects.
    // Used to nodify Sample objects. Do not marshal/unmarshal.
    private Sample mNext;
    private static Sample sPool = new Sample();
    private static int sPoolSize = 1;

    public Sample() { }

    private void readInfo(final Parcel in) {
        int offset = in.readInt();
        int size = in.readInt();
        long pts = in.readLong();
        int flags = in.readInt();

        info.set(offset, size, pts, flags);
    }

    private void readCrypto(final Parcel in) {
        int hasCryptoInfo = in.readInt();
        if (hasCryptoInfo == 0) {
            cryptoInfo = null;
            return;
        }

        byte[] iv = in.createByteArray();
        byte[] key = in.createByteArray();
        int mode = in.readInt();
        int[] numBytesOfClearData = in.createIntArray();
        int[] numBytesOfEncryptedData = in.createIntArray();
        int numSubSamples = in.readInt();

        if (cryptoInfo == null) {
            cryptoInfo = new CryptoInfo();
        }
        cryptoInfo.set(numSubSamples,
                      numBytesOfClearData,
                      numBytesOfEncryptedData,
                      key,
                      iv,
                      mode);
    }

    public Sample set(final BufferInfo info, final CryptoInfo cryptoInfo) {
        setBufferInfo(info);
        setCryptoInfo(cryptoInfo);
        return this;
    }

    public void setBufferInfo(final BufferInfo info) {
        this.info.set(0, info.size, info.presentationTimeUs, info.flags);
    }

    public void setCryptoInfo(final CryptoInfo crypto) {
        if (crypto == null) {
            cryptoInfo = null;
            return;
        }

        if (cryptoInfo == null) {
            cryptoInfo = new CryptoInfo();
        }
        cryptoInfo.set(crypto.numSubSamples,
                       crypto.numBytesOfClearData,
                       crypto.numBytesOfEncryptedData,
                       crypto.key,
                       crypto.iv,
                       crypto.mode);
    }

    @WrapForJNI
    public void dispose() {
        if (isEOS()) {
            return;
        }

        bufferId = NO_BUFFER;
        info.set(0, 0, 0, 0);
        if (cryptoInfo != null) {
            cryptoInfo.set(0, null, null, null, null, 0);
        }

        // Recycle it.
        synchronized (CREATOR) {
            this.mNext = sPool;
            sPool = this;
            sPoolSize++;
        }
    }

    public boolean isEOS() {
        return (this == EOS) ||
                ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0);
    }

    public static final Creator<Sample> CREATOR = new Creator<Sample>() {
        @Override
        public Sample createFromParcel(final Parcel in) {
            return obtainSample(in);
        }

        @Override
        public Sample[] newArray(final int size) {
            return new Sample[size];
        }

        private synchronized Sample obtainSample(final Parcel in) {
            Sample s = null;
            if (sPoolSize > 0) {
                s = sPool;
                sPool = s.mNext;
                s.mNext = null;
                sPoolSize--;
            } else {
                s = new Sample();
            }
            s.session = in.readLong();
            s.bufferId = in.readInt();
            s.readInfo(in);
            s.readCrypto(in);
            return s;
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(final Parcel dest, final int parcelableFlags) {
        dest.writeLong(session);
        dest.writeInt(bufferId);
        writeInfo(dest);
        writeCrypto(dest);
    }

    private void writeInfo(final Parcel dest) {
        dest.writeInt(info.offset);
        dest.writeInt(info.size);
        dest.writeLong(info.presentationTimeUs);
        dest.writeInt(info.flags);
    }

    private void writeCrypto(final Parcel dest) {
        if (cryptoInfo != null) {
            dest.writeInt(1);
            dest.writeByteArray(cryptoInfo.iv);
            dest.writeByteArray(cryptoInfo.key);
            dest.writeInt(cryptoInfo.mode);
            dest.writeIntArray(cryptoInfo.numBytesOfClearData);
            dest.writeIntArray(cryptoInfo.numBytesOfEncryptedData);
            dest.writeInt(cryptoInfo.numSubSamples);
        } else {
            dest.writeInt(0);
        }
    }

    public static byte[] byteArrayFromBuffer(final ByteBuffer buffer, final int offset,
                                             final int size) {
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

    @Override
    public String toString() {
        if (isEOS()) {
            return "EOS sample";
        }

        StringBuilder str = new StringBuilder();
        str.append("{ session#:").append(session).
                append(", buffer#").append(bufferId).
                append(", info=").
                append("{ offset=").append(info.offset).
                append(", size=").append(info.size).
                append(", pts=").append(info.presentationTimeUs).
                append(", flags=").append(Integer.toHexString(info.flags)).append(" }").
                append(" }");
        return str.toString();
    }
}
