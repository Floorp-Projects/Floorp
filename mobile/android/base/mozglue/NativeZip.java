/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import java.io.InputStream;
import java.nio.ByteBuffer;

public class NativeZip implements NativeReference {
    private static final int DEFLATE = 8;
    private static final int STORE = 0;

    private volatile long mObj;
    private InputStream mInput;

    public NativeZip(String path) {
        mObj = getZip(path);
        mInput = null;
    }

    public NativeZip(InputStream input) {
        if (!(input instanceof ByteBufferInputStream)) {
            throw new IllegalArgumentException("Got " + input.getClass()
                                               + ", but expected ByteBufferInputStream!");
        }
        ByteBufferInputStream bbinput = (ByteBufferInputStream)input;
        mObj = getZipFromByteBuffer(bbinput.mBuf);
        mInput = input;
    }

    @Override
    public void finalize() {
        release();
    }

    public void close() {
        release();
    }

    @Override
    public void release() {
        if (mObj != 0) {
            _release(mObj);
            mObj = 0;
        }
        mInput = null;
    }

    @Override
    public boolean isReleased() {
        return (mObj == 0);
    }

    public InputStream getInputStream(String path) {
        if (isReleased()) {
            throw new IllegalStateException("Can't get path \"" + path
                                            + "\" because NativeZip is closed!");
        }
        return _getInputStream(mObj, path);
    }

    private static native long getZip(String path);
    private static native long getZipFromByteBuffer(ByteBuffer buffer);
    private static native void _release(long obj);
    private native InputStream _getInputStream(long obj, String path);

    private InputStream createInputStream(ByteBuffer buffer, int compression) {
        if (compression != STORE) {
            throw new IllegalArgumentException("Got compression " + compression + ", but expected 0 (STORE)!");
        }
        return new ByteBufferInputStream(buffer, this);
    }
}
