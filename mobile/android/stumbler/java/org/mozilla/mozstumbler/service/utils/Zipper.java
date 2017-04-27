/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.utils;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

public class Zipper {
    public static byte[] zipData(byte[] data) throws IOException {
        final ByteArrayOutputStream os = new ByteArrayOutputStream();
        GZIPOutputStream gstream = new GZIPOutputStream(os);
        byte[] output;
        try {
            gstream.write(data);
            gstream.finish();
            output = os.toByteArray();
        } finally {
            gstream.close();
            os.close();
        }
        return output;
    }

    public static String unzipData(byte[] data) throws IOException {
        StringBuilder result = new StringBuilder();
        final ByteArrayInputStream bs = new ByteArrayInputStream(data);
        GZIPInputStream gstream = new GZIPInputStream(bs);
        try {
            InputStreamReader reader = new InputStreamReader(gstream, StringUtils.UTF_8);
            BufferedReader in = new BufferedReader(reader);
            String read;
            while ((read = in.readLine()) != null) {
                result.append(read);
            }
        } finally {
            gstream.close();
            bs.close();
        }
        return result.toString();
    }
}
