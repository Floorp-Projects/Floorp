/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.support.annotation.NonNull;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Pair a (String) value with a timestamp.  The timestamp is usually when the
 * value was fetched from a remote service or when the value was locally
 * generated.
 *
 * It's awkward to serialize generic values to JSON -- that requires lots of
 * factory classes -- so we specialize to String instances.
 */
public class Fetched {
    public final String value;
    public final long timestamp;

    public Fetched(String value, long timestamp) {
        this.value = value;
        this.timestamp = timestamp;
    }

    public static Fetched now(String value) {
        return new Fetched(value, System.currentTimeMillis());
    }

    public static @NonNull Fetched fromJSONObject(@NonNull JSONObject json) {
        final String value = json.optString("value", null);
        final String timestampString = json.optString("timestamp", null);
        final long timestamp = timestampString != null ? Long.valueOf(timestampString) : 0L;
        return new Fetched(value, timestamp);
    }

    public JSONObject toJSONObject() throws JSONException {
        final JSONObject jsonObject = new JSONObject();
        if (value != null) {
            jsonObject.put("value", value);
        } else {
            jsonObject.remove("value");
        }
        jsonObject.put("timestamp", Long.toString(timestamp));
        return jsonObject;
    }

    @Override
    public boolean equals(Object o) {
        // Auto-generated.
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Fetched fetched = (Fetched) o;

        if (timestamp != fetched.timestamp) return false;
        return !(value != null ? !value.equals(fetched.value) : fetched.value != null);

    }

    @Override
    public int hashCode() {
        // Auto-generated.
        int result = value != null ? value.hashCode() : 0;
        result = 31 * result + (int) (timestamp ^ (timestamp >>> 32));
        return result;
    }
}
