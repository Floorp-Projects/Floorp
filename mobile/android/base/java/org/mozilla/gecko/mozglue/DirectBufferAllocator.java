/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mozglue;

import java.nio.ByteBuffer;

//
// We must manually allocate direct buffers in JNI to work around a bug where Honeycomb's
// ByteBuffer.allocateDirect() grossly overallocates the direct buffer size.
// https://code.google.com/p/android/issues/detail?id=16941
//

public final class DirectBufferAllocator {
    private DirectBufferAllocator() {}

    public static ByteBuffer allocate(int size) {
        if (size <= 0) {
            throw new IllegalArgumentException("Invalid size " + size);
        }

        ByteBuffer directBuffer = nativeAllocateDirectBuffer(size);
        if (directBuffer == null) {
            throw new OutOfMemoryError("allocateDirectBuffer() returned null");
        }

        if (!directBuffer.isDirect()) {
            throw new AssertionError("allocateDirectBuffer() did not return a direct buffer");
        }

        return directBuffer;
    }

    public static ByteBuffer free(ByteBuffer buffer) {
        if (buffer == null) {
            return null;
        }

        if (!buffer.isDirect()) {
            throw new IllegalArgumentException("buffer must be direct");
        }

        nativeFreeDirectBuffer(buffer);
        return null;
    }

    // These JNI methods are implemented in mozglue/android/nsGeckoUtils.cpp.
    private static native ByteBuffer nativeAllocateDirectBuffer(long size);
    private static native void nativeFreeDirectBuffer(ByteBuffer buf);
}
