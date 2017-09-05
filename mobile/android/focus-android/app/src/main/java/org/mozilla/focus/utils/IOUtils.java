/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

public class IOUtils {
    public static JSONObject readAsset(Context context, String fileName) throws IOException {
        try (final BufferedReader reader =
                     new BufferedReader(new InputStreamReader(context.getAssets().open(fileName), StandardCharsets.UTF_8))) {
            final StringBuilder builder = new StringBuilder();
            String line;

            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }

            return new JSONObject(builder.toString());
        } catch (JSONException e) {
            throw new AssertionError("Corrupt JSON asset (" + fileName + ")", e);
        }
    }
}
