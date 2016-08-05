/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import java.io.IOException;

public final class AsyncCodecFactory {
    public static AsyncCodec create(String name) throws IOException {
        // TODO: create (to be implemented) LollipopAsyncCodec when running on Lollipop or later devices.
        return new JellyBeanAsyncCodec(name);
    }
}
