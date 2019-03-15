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

/**
 * {@code RawResource} provides API to load raw resources in different
 * forms. For now, we only load them as strings. We're using raw resources
 * as localizable 'assets' as opposed to a string that can be directly
 * translatable e.g. JSON file vs string.
 *
 * This is just a utility class to avoid code duplication for the different
 * cases where need to read such assets.
 */
public final class RawResource {
    public static String getAsString(Context context, int id) throws IOException {
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