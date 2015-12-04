/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import java.io.InputStream;
import java.nio.ByteBuffer;

class ByteBufferInputStream extends InputStream {

    protected ByteBuffer mBuf;
    // Reference to a native object holding the data backing the ByteBuffer.
    private final NativeReference mNativeRef;

    protected ByteBufferInputStream(ByteBuffer buffer, NativeReference ref) {
        mBuf = buffer;
        mNativeRef = ref;
    }

    @Override
    public int available() {
        return mBuf.remaining();
    }

    @Override
    public void close() {
        mBuf = null;
        mNativeRef.release();
    }

    @Override
    public int read() {
        if (!mBuf.hasRemaining() || mNativeRef.isReleased()) {
            return -1;
        }

        return mBuf.get() & 0xff; // Avoid sign extension
    }

    @Override
    public int read(byte[] buffer, int offset, int length) {
        if (!mBuf.hasRemaining() || mNativeRef.isReleased()) {
            return -1;
        }

        length = Math.min(length, mBuf.remaining());
        mBuf.get(buffer, offset, length);
        return length;
    }

    @Override
    public long skip(long byteCount) {
        if (byteCount < 0 || mNativeRef.isReleased()) {
            return 0;
        }

        byteCount = Math.min(byteCount, mBuf.remaining());
        mBuf.position(mBuf.position() + (int)byteCount);
        return byteCount;
    }

}
