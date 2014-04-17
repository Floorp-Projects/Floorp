/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.util.UUID;

import org.json.JSONException;
import org.json.JSONObject;

public final class JSONUtils {
    private JSONUtils() {}

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
