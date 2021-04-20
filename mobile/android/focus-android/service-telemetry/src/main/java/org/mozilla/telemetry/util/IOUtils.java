/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.util;

import androidx.annotation.RestrictTo;

import java.io.Closeable;
import java.io.IOException;

@RestrictTo(RestrictTo.Scope.LIBRARY)
public class IOUtils {
    public static void safeClose(Closeable stream) {
        try {
            if (stream != null) {
                stream.close();
            }
        } catch (IOException ignored) { }
    }
}
