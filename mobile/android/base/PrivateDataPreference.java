/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;

import org.json.JSONObject;
import org.json.JSONException;

import java.util.Map;

import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.util.AttributeSet;
import android.util.Log;

class PrivateDataPreference extends MultiChoicePreference {
    private static final String LOGTAG = "GeckoPrivateDataPreference";
    private static final String PREF_KEY_PREFIX = "private.data.";

    private Context mContext;

    public PrivateDataPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);

        if (!positiveResult)
            return;

        CharSequence keys[] = getEntryKeys();
        boolean values[] = getValues();
        JSONObject json = new JSONObject();

        for (int i = 0; i < keys.length; i++) {
            // Privacy pref checkbox values are stored in Android prefs to
            // remember their check states. The key names are private.data.X,
            // where X is a string from Gecko sanitization. This prefix is
            // removed here so we can send the values to Gecko, which then does
            // the sanitization for each key.
            String key = keys[i].toString().substring(PREF_KEY_PREFIX.length());
            boolean value = values[i];
            try {
                json.put(key, value);
            } catch (JSONException e) {
                Log.e(LOGTAG, "JSON error", e);
            }
        }

        // clear private data in gecko
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Sanitize:ClearData", json.toString()));
    }
}
