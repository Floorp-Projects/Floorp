/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.os.Build;

import java.io.IOException;

public final class AsyncCodecFactory {
    public static AsyncCodec create(String name) throws IOException {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
               ? new LollipopAsyncCodec(name)
               : new JellyBeanAsyncCodec(name);
    }
}
