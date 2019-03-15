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
import org.mozilla.gecko.mozglue.SharedMemory;

import java.io.IOException;
import java.nio.ByteBuffer;

// Parcelable carrying input/output sample data and info cross process.
public final class Sample implements Parcelable {
    public static final Sample EOS;
    static {
        BufferInfo eosInfo = new BufferInfo();
        eosInfo.set(0, 0, Long.MIN_VALUE, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
        EOS = new Sample(null, eosInfo, null);
    }

    public interface Buffer extends Parcelable {
        int capacity();
        void readFromByteBuffer(ByteBuffer src, int offset, int size) throws IOException;
        void writeToByteBuffer(ByteBuffer dest, int offset, int size) throws IOException;
        void dispose();
    }

    private static final class ArrayBuffer implements Buffer {
        private byte[] mArray;

        public static final Creator<ArrayBuffer> CREATOR = new Creator<ArrayBuffer>() {
            @Override
            public ArrayBuffer createFromParcel(final Parcel in) {
                return new ArrayBuffer(in);
            }

            @Override
            public ArrayBuffer[] newArray(final int size) {
                return new ArrayBuffer[size];
            }
        };

        private ArrayBuffer(final Parcel in) {
            mArray = in.createByteArray();
        }

        private ArrayBuffer(final byte[] bytes) {
            mArray = bytes;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(final Parcel dest, final int flags) {
            dest.writeByteArray(mArray);
        }

        @Override
        public int capacity() {
            return mArray != null ? mArray.length : 0;
        }

        @Override
        public void readFromByteBuffer(final ByteBuffer src, final int offset, final int size)
                throws IOException {
            src.position(offset);
            if (mArray == null || mArray.length != size) {
                mArray = new byte[size];
            }
            src.get(mArray, 0, size);
        }

        @Override
        public void writeToByteBuffer(final ByteBuffer dest, final int offset, final int size)
                throws IOException {
            dest.put(mArray, offset, size);
        }

        @Override
        public void dispose() {
            mArray = null;
        }
    }

    public Buffer buffer;
    @WrapForJNI
    public BufferInfo info;
    public CryptoInfo cryptoInfo;

    public static Sample create() {
        return create(null, new BufferInfo(), null);
    }

    public static Sample create(final ByteBuffer src, final BufferInfo info,
                                final CryptoInfo cryptoInfo) {
        ArrayBuffer buffer = new ArrayBuffer(byteArrayFromBuffer(src, info.offset, info.size));

        BufferInfo bufferInfo = new BufferInfo();
        bufferInfo.set(0, info.size, info.presentationTimeUs, info.flags);

        return new Sample(buffer, bufferInfo, cryptoInfo);
    }

    public static Sample create(final SharedMemory sharedMem) {
        return new Sample(new SharedMemBuffer(sharedMem), new BufferInfo(), null);
    }

    private Sample(final Buffer bytes, final BufferInfo info, final CryptoInfo cryptoInfo) {
        buffer = bytes;
        this.info = info;
        this.cryptoInfo = cryptoInfo;
    }

    private Sample(final Parcel in) {
        readInfo(in);
        readCrypto(in);
        buffer = in.readParcelable(Sample.class.getClassLoader());
    }

    private void readInfo(final Parcel in) {
        int offset = in.readInt();
        int size = in.readInt();
        long pts = in.readLong();
        int flags = in.readInt();

        info = new BufferInfo();
        info.set(offset, size, pts, flags);
    }

    private void readCrypto(final Parcel in) {
        int hasCryptoInfo = in.readInt();
        if (hasCryptoInfo == 0) {
            return;
        }

        byte[] iv = in.createByteArray();
        byte[] key = in.createByteArray();
        int mode = in.readInt();
        int[] numBytesOfClearData = in.createIntArray();
        int[] numBytesOfEncryptedData = in.createIntArray();
        int numSubSamples = in.readInt();

        cryptoInfo = new CryptoInfo();
        cryptoInfo.set(numSubSamples,
                      numBytesOfClearData,
                      numBytesOfEncryptedData,
                      key,
                      iv,
                      mode);
    }

    public Sample set(final ByteBuffer bytes, final BufferInfo info, final CryptoInfo cryptoInfo)
            throws IOException {
        if (bytes != null && info.size > 0) {
            buffer.readFromByteBuffer(bytes, info.offset, info.size);
        }
        this.info.set(0, info.size, info.presentationTimeUs, info.flags);
        this.cryptoInfo = cryptoInfo;

        return this;
    }

    public void dispose() {
        if (isEOS()) {
            return;
        }

        if (buffer != null) {
            buffer.dispose();
            buffer = null;
        }
        info = null;
        cryptoInfo = null;
    }

    public boolean isEOS() {
        return (this == EOS) ||
                ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0);
    }

    public static final Creator<Sample> CREATOR = new Creator<Sample>() {
        @Override
        public Sample createFromParcel(final Parcel in) {
            return new Sample(in);
        }

        @Override
        public Sample[] newArray(final int size) {
            return new Sample[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(final Parcel dest, final int parcelableFlags) {
        writeInfo(dest);
        writeCrypto(dest);
        dest.writeParcelable(buffer, parcelableFlags);
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

    @WrapForJNI
    public void writeToByteBuffer(final ByteBuffer dest) throws IOException {
        if (buffer != null && dest != null && info.size > 0) {
            buffer.writeToByteBuffer(dest, info.offset, info.size);
        }
    }

    @Override
    public String toString() {
        if (isEOS()) {
            return "EOS sample";
        }

        StringBuilder str = new StringBuilder();
        str.append("{ buffer=").append(buffer).
                append(", info=").
                append("{ offset=").append(info.offset).
                append(", size=").append(info.size).
                append(", pts=").append(info.presentationTimeUs).
                append(", flags=").append(Integer.toHexString(info.flags)).append(" }").
                append(" }");
        return str.toString();
    }
}
