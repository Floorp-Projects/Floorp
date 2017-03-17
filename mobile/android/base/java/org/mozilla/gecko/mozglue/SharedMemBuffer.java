/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.mozglue;

import android.os.Parcel;

import org.mozilla.gecko.media.Sample;

import java.io.IOException;
import java.nio.ByteBuffer;

public final class SharedMemBuffer implements Sample.Buffer {
    private SharedMemory mSharedMem;

    /* package */
    public SharedMemBuffer(SharedMemory sharedMem) {
        mSharedMem = sharedMem;
    }

    protected SharedMemBuffer(Parcel in) {
        mSharedMem = in.readParcelable(Sample.class.getClassLoader());
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeParcelable(mSharedMem, flags);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public static final Creator<SharedMemBuffer> CREATOR = new Creator<SharedMemBuffer>() {
        @Override
        public SharedMemBuffer createFromParcel(Parcel in) {
            return new SharedMemBuffer(in);
        }

        @Override
        public SharedMemBuffer[] newArray(int size) {
            return new SharedMemBuffer[size];
        }
    };

    @Override
    public int capacity() {
        return mSharedMem != null ? mSharedMem.getSize() : 0;
    }

    @Override
    public void readFromByteBuffer(ByteBuffer src, int offset, int size) throws IOException {
        if (!src.isDirect()) {
            throw new IOException("SharedMemBuffer only support reading from direct byte buffer.");
        }
        try {
            nativeReadFromDirectBuffer(src, mSharedMem.getPointer(), offset, size);
        } catch (NullPointerException e) {
            throw new IOException(e);
        }
    }

    private native static void nativeReadFromDirectBuffer(ByteBuffer src, long dest, int offset, int size);

    @Override
    public void writeToByteBuffer(ByteBuffer dest, int offset, int size) throws IOException {
        if (!dest.isDirect()) {
            throw new IOException("SharedMemBuffer only support writing to direct byte buffer.");
        }
        try {
            nativeWriteToDirectBuffer(mSharedMem.getPointer(), dest, offset, size);
        } catch (NullPointerException e) {
            throw new IOException(e);
        }
    }

    private native static void nativeWriteToDirectBuffer(long src, ByteBuffer dest, int offset, int size);

    @Override
    public void dispose() {
        if (mSharedMem != null) {
            mSharedMem.dispose();
            mSharedMem = null;
        }
    }

    @Override public String toString() { return "Buffer: " + mSharedMem; }
}
