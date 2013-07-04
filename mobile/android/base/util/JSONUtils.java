/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.UUID;

public final class JSONUtils {
    private static final String LOGTAG = "JSONUtils";

    private JSONUtils() {}

    public static URL getURL(String name, JSONObject json) {
        String url = json.optString(name, null);
        if (url == null) {
            return null;
        }

        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            Log.e(LOGTAG, "", new IllegalStateException(name + "=" + url, e));
            return null;
        }
    }

    public static void putURL(String name, URL url, JSONObject json) {
        String urlString = url.toString();
        try {
            json.put(name, urlString);
        } catch (JSONException e) {
            throw new IllegalArgumentException(name + "=" + urlString, e);
        }
    }

    public static UUID getUUID(String name, JSONObject json) {
        String uuid = json.optString(name, null);
        return (uuid != null) ? UUID.fromString(uuid) : null;
    }

    public static void putUUID(String name, UUID uuid, JSONObject json) {
        String uuidString = uuid.toString();
        try {
            json.put(name, uuidString);
        } catch (JSONException e) {
            throw new IllegalArgumentException(name + "=" + uuidString, e);
        }
    }
}
