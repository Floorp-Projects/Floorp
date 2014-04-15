/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;

import java.io.IOException;
import java.io.InputStream;

public final class RawResource {
    public static String get(Context context, int id) throws IOException {
        final InputStream inputStream = context.getResources().openRawResource(id);
        final byte[] buffer = new byte[1024];
        StringBuilder s = new StringBuilder();
        int count;

        while ((count = inputStream.read(buffer)) != -1) {
            s.append(new String(buffer, 0, count));
        }

        return s.toString();
    }
}