/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.telemetry;


import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.sync.SharedPreferencesClientsDataDelegate;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.util.StringUtils;

import java.io.UnsupportedEncodingException;
import java.nio.charset.Charset;
import java.security.GeneralSecurityException;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.Map;

public class TelemetryEventCollector {
    private static final String LOG_TAG = "TelemetryEventCollector";

    private static final String ACTION_BACKGROUND_TELEMETRY = "org.mozilla.gecko.telemetry.BACKGROUND";
    private static final String EVENT_CATEGORY_SYNC = "sync"; // Max byte length: 30.

    public static void recordEvent(final Context context, final String object, final String method,
                                   @Nullable final String value,
                                   @Nullable final HashMap<String, String> extra) {
        if (!validateTelemetryEvent(object, method, value, extra)) {
            throw new IllegalArgumentException("Could not validate telemetry event.");
        }

        Log.d(LOG_TAG, "Recording event {" + object + ", " + method + ", " + value + ", " + extra);
        final Bundle event = new Bundle();
        event.putLong(TelemetryContract.KEY_EVENT_TIMESTAMP, SystemClock.elapsedRealtime());
        event.putString(TelemetryContract.KEY_EVENT_CATEGORY, EVENT_CATEGORY_SYNC);
        event.putString(TelemetryContract.KEY_EVENT_OBJECT, object);
        event.putString(TelemetryContract.KEY_EVENT_METHOD, method);
        if (value != null) {
            event.putString(TelemetryContract.KEY_EVENT_VALUE, value);
        }
        if (extra != null) {
            final Bundle extraBundle = new Bundle();
            for (Map.Entry<String, String> e : extra.entrySet()) {
                extraBundle.putString(e.getKey(), e.getValue());
            }
            event.putBundle(TelemetryContract.KEY_EVENT_EXTRA, extraBundle);
        }
        if (!setIDs(context, event)) {
            throw new IllegalStateException("UID and deviceID need to be set.");
        }

        final Intent telemetryIntent = new Intent();
        telemetryIntent.setAction(ACTION_BACKGROUND_TELEMETRY);
        telemetryIntent.putExtra(TelemetryContract.KEY_TYPE, TelemetryContract.KEY_TYPE_EVENT);
        telemetryIntent.putExtra(TelemetryContract.KEY_TELEMETRY, event);
        LocalBroadcastManager.getInstance(context).sendBroadcast(telemetryIntent);
    }

    // The Firefox Telemetry pipeline imposes size limits.
    // See toolkit/components/telemetry/docs/collection/events.rst
    @VisibleForTesting
    static boolean validateTelemetryEvent(final String object, final String method,
                                          @Nullable final String value, @Nullable final HashMap<String, String> extra) {
        // The Telemetry Sender uses BaseResource under the hood.
        final Charset charset = Charset.forName(BaseResource.charset);
        // Length checks.
        if (method.getBytes(charset).length > 20 ||
                object.getBytes(charset).length > 20 ||
                (value != null && value.getBytes(charset).length > 80)) {
            Log.w(LOG_TAG, "Invalid event parameters - wrong lengths: " + method + " " +
                    object + " " + value);
            return false;
        }

        if (extra != null) {
            if (extra.size() > 10) {
                Log.w(LOG_TAG, "Invalid event parameters - too many extra keys: " + extra);
                return false;
            }
            for (Map.Entry<String, String> e : extra.entrySet()) {
                if (e.getKey().getBytes(charset).length > 15 ||
                    e.getValue().getBytes(charset).length > 80) {
                    Log.w(LOG_TAG, "Invalid event parameters: extra item \"" + e.getKey() +
                            "\" is invalid: " + extra);
                    return false;
                }
            }
        }

        return true;
    }

    private static boolean setIDs(final Context context, final Bundle event) {
        final SharedPreferences sharedPrefs;
        final AndroidFxAccount fxAccount = AndroidFxAccount.fromContext(context);
        if (fxAccount == null) {
            Log.e(LOG_TAG, "Can't record telemetry event: FxAccount doesn't exist.");
            return false;
        }
        try {
            sharedPrefs = fxAccount.getSyncPrefs();
        }  catch (UnsupportedEncodingException | GeneralSecurityException e) {
            Log.e(LOG_TAG, "Can't record telemetry event: Could not retrieve Sync Prefs", e);
            return false;
        }

        final String hashedFxAUID = fxAccount.getCachedHashedFxAUID();
        if (TextUtils.isEmpty(hashedFxAUID)) {
            Log.e(LOG_TAG, "Can't record telemetry event: The hashed FxA UID is empty");
            return false;
        }

        final ClientsDataDelegate clientsDataDelegate = new SharedPreferencesClientsDataDelegate(sharedPrefs, context);
        try {
            final String hashedDeviceID = Utils.byte2Hex(Utils.sha256(
                    clientsDataDelegate.getAccountGUID().concat(hashedFxAUID).getBytes(StringUtils.UTF_8)
            ));
            event.putString(TelemetryContract.KEY_LOCAL_DEVICE_ID, hashedDeviceID);
        } catch (NoSuchAlgorithmException e) {
            // Should not happen.
            Log.e(LOG_TAG, "Either UTF-8 or SHA-256 are not supported", e);
            return false;
        }

        event.putString(TelemetryContract.KEY_LOCAL_UID, hashedFxAUID);
        return true;
    }
}
