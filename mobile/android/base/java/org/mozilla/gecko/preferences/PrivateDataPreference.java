/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.util.GeckoBundle;

import org.mozilla.gecko.icons.storage.DiskStorage;

import java.util.Arrays;
import java.util.Collections;
import java.util.Set;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;

class PrivateDataPreference extends MultiPrefMultiChoicePreference {
    private static final String LOGTAG = "GeckoPrivateDataPreference";
    private static final String PREF_KEY_PREFIX = "private.data.";

    public PrivateDataPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);

        if (!positiveResult) {
            return;
        }

        Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.DIALOG, "settings");

        final Set<String> values = getValues();
        final GeckoBundle data = new GeckoBundle();

        for (String value : values) {
            // Privacy pref checkbox values are stored in Android prefs to
            // remember their check states. The key names are private.data.X,
            // where X is a string from Gecko sanitization. This prefix is
            // removed here so we can send the values to Gecko, which then does
            // the sanitization for each key.
            final String key = value.substring(PREF_KEY_PREFIX.length());
            data.putBoolean(key, true);
        }

        if (values.contains("private.data.offlineApps")) {
            // Remove all icons from storage if removing "Offline website data" was selected.
            DiskStorage.get(getContext()).evictAll();
        }

        // clear private data in gecko
        EventDispatcher.getInstance().dispatch("Sanitize:ClearData", data);
    }
}
