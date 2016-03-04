/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gcm;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.android.gms.iid.InstanceID;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.push.Fetched;

import java.io.IOException;

/**
 * Fetch and cache GCM tokens.
 * <p/>
 * GCM tokens are stable and long lived.  Google Play Services will periodically request that
 * they are rotated, however: see
 * <a href="https://developers.google.com/instance-id/guides/android-implementation">https://developers.google.com/instance-id/guides/android-implementation</a>.
 * <p/>
 * The GCM token is cached in the App-wide shared preferences.  There's no particular harm in
 * requesting new tokens, so if the user clears the App data, that's fine -- we'll get a fresh
 * token and Push will react accordingly.
 */
public class GcmTokenClient {
    private static final String LOG_TAG = "GeckoPushGCM";

    private static final String KEY_GCM_TOKEN = "gcm_token";
    private static final String KEY_GCM_TOKEN_TIMESTAMP = "gcm_token_timestamp";

    private final Context context;

    public GcmTokenClient(Context context) {
        this.context = context;
    }

    /**
     * Check the device to make sure it has the Google Play Services APK.
     * @param context Android context.
     */
    protected void ensurePlayServices(Context context) throws NeedsGooglePlayServicesException {
        final GoogleApiAvailability apiAvailability = GoogleApiAvailability.getInstance();
        int resultCode = apiAvailability.isGooglePlayServicesAvailable(context);
        if (resultCode != ConnectionResult.SUCCESS) {
            Log.w(LOG_TAG, "This device does not support GCM! isGooglePlayServicesAvailable returned: " + resultCode);
            Log.w(LOG_TAG, "isGooglePlayServicesAvailable message: " + apiAvailability.getErrorString(resultCode));
            throw new NeedsGooglePlayServicesException(resultCode);
        }
    }

    /**
     * Get a GCM token (possibly cached).
     *
     * @param senderID to request token for.
     * @param debug whether to log debug details.
     * @return token and timestamp.
     * @throws NeedsGooglePlayServicesException if user action is needed to use Google Play Services.
     * @throws IOException if the token fetch failed.
     */
    public @NonNull Fetched getToken(@NonNull String senderID, boolean debug) throws NeedsGooglePlayServicesException, IOException {
        ensurePlayServices(this.context);

        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(context);
        String token = sharedPrefs.getString(KEY_GCM_TOKEN, null);
        long timestamp = sharedPrefs.getLong(KEY_GCM_TOKEN_TIMESTAMP, 0L);
        if (token != null && timestamp > 0L) {
            if (debug) {
                Log.i(LOG_TAG, "Cached GCM token exists: " + token);
            } else {
                Log.i(LOG_TAG, "Cached GCM token exists.");
            }
            return new Fetched(token, timestamp);
        }

        Log.i(LOG_TAG, "Cached GCM token does not exist; requesting new token with sender ID: " + senderID);

        final InstanceID instanceID = InstanceID.getInstance(context);
        token = instanceID.getToken(senderID, GoogleCloudMessaging.INSTANCE_ID_SCOPE, null);
        timestamp = System.currentTimeMillis();

        if (debug) {
            Log.i(LOG_TAG, "Got fresh GCM token; caching: " + token);
        } else {
            Log.i(LOG_TAG, "Got fresh GCM token; caching.");
        }
        sharedPrefs
                .edit()
                .putString(KEY_GCM_TOKEN, token)
                .putLong(KEY_GCM_TOKEN_TIMESTAMP, timestamp)
                .apply();

        return new Fetched(token, timestamp);
    }

    /**
     * Remove any cached GCM token.
     */
    public void invalidateToken() {
        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(context);
        sharedPrefs
                .edit()
                .remove(KEY_GCM_TOKEN)
                .remove(KEY_GCM_TOKEN_TIMESTAMP)
                .apply();
    }

    public class NeedsGooglePlayServicesException extends Exception {
        private static final long serialVersionUID = 4132853166L;

        private final int resultCode;

        NeedsGooglePlayServicesException(int resultCode) {
            super();
            this.resultCode = resultCode;
        }

        public void showErrorNotification() {
            final GoogleApiAvailability apiAvailability = GoogleApiAvailability.getInstance();
            apiAvailability.showErrorNotification(context, resultCode);
        }
    }
}
