/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.media;

import android.os.Parcel;

import org.mozilla.gecko.mozglue.SharedMemory;
import org.mozilla.gecko.media.Sample;

import java.io.IOException;
import java.nio.ByteBuffer;

public final class SharedMemBuffer implements Sample.Buffer {
    private SharedMemory mSharedMem;

    /* package */
    public SharedMemBuffer(final SharedMemory sharedMem) {
        mSharedMem = sharedMem;
    }

    protected SharedMemBuffer(final Parcel in) {
        mSharedMem = in.readParcelable(Sample.class.getClassLoader());
    }

    @Override
    public void writeToParcel(final Parcel dest, final int flags) {
        dest.writeParcelable(mSharedMem, flags);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public static final Creator<SharedMemBuffer> CREATOR = new Creator<SharedMemBuffer>() {
        @Override
        public SharedMemBuffer createFromParcel(final Parcel in) {
            return new SharedMemBuffer(in);
        }

        @Override
        public SharedMemBuffer[] newArray(final int size) {
            return new SharedMemBuffer[size];
        }
    };

    @Override
    public int capacity() {
        return mSharedMem != null ? mSharedMem.getSize() : 0;
    }

    @Override
    public void readFromByteBuffer(final ByteBuffer src, final int offset, final int size)
            throws IOException {
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
    public void writeToByteBuffer(final ByteBuffer dest, final int offset, final int size)
            throws IOException {
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

    @Override public String toString() {
        return "Buffer: " + mSharedMem;
    }
}
