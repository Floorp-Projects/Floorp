/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream.model;

import android.database.Cursor;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract;

public class Metadata {
    private static final String LOGTAG = "GeckoMetadata";

    public static Metadata fromCursor(Cursor cursor) {
        return new Metadata(
                cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Highlights.METADATA)));
    }

    private String provider;

    private Metadata(String json) {
        if (TextUtils.isEmpty(json)) {
            // Just use default values. It's better to have an empty Metadata object instead of
            // juggling with null values.
            return;
        }

        try {
            JSONObject object = new JSONObject(json);

            provider = object.optString("provider");
        } catch (JSONException e) {
            Log.w(LOGTAG, "JSONException while parsing metadata", e);
        }
    }

    public boolean hasProvider() {
        return !TextUtils.isEmpty(provider);
    }

    public String getProvider() {
        return provider;
    }
}
