/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.res.Resources;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.StringWriter;

public final class RawResource {
    public static String get(Context context, int id) throws IOException {
        InputStreamReader reader = null;

        try {
            final Resources res = context.getResources();
            final InputStream is = res.openRawResource(id);
            if (is == null) {
                return null;
            }

            reader = new InputStreamReader(is);

            final char[] buffer = new char[1024];
            final StringWriter s = new StringWriter();

            int n;
            while ((n = reader.read(buffer, 0, buffer.length)) != -1) {
                s.write(buffer, 0, n);
            }

            return s.toString();
        } finally {
            if (reader != null) {
                reader.close();
            }
        }
    }
}