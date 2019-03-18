/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.util.Log;

import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

public final class JSONUtils {
    private static final String LOGTAG = "GeckoJSONUtils";

    private JSONUtils() {}

    public static UUID getUUID(final String name, final JSONObject json) {
        String uuid = json.optString(name, null);
        return (uuid != null) ? UUID.fromString(uuid) : null;
    }

    public static void putUUID(final String name, final UUID uuid, final JSONObject json) {
        String uuidString = uuid.toString();
        try {
            json.put(name, uuidString);
        } catch (JSONException e) {
            throw new IllegalArgumentException(name + "=" + uuidString, e);
        }
    }

    public static JSONObject bundleToJSON(final Bundle bundle) {
        if (bundle == null || bundle.isEmpty()) {
            return null;
        }

        JSONObject json = new JSONObject();
        for (String key : bundle.keySet()) {
            try {
                json.put(key, bundle.get(key));
            } catch (JSONException e) {
                Log.w(LOGTAG, "Error building JSON response.", e);
            }
        }

        return json;
    }

    // Handles conversions between a JSONArray and a Set<String>
    public static Set<String> parseStringSet(final JSONArray json) {
        final Set<String> ret = new HashSet<String>();

        for (int i = 0; i < json.length(); i++) {
            try {
                ret.add(json.getString(i));
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing json", ex);
            }
        }

        return ret;
    }

}
