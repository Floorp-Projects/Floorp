/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.os.Build;

import java.io.IOException;

public final class AsyncCodecFactory {
    public static AsyncCodec create(final String name) throws IOException {
        // A bug that getInputBuffer() could fail after flush() then start() wasn't fixed until MR1.
        // See: https://android.googlesource.com/platform/frameworks/av/+/d9e0603a1be07dbb347c55050c7d4629ea7492e8
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1
               ? new LollipopAsyncCodec(name)
               : new JellyBeanAsyncCodec(name);
    }
}
