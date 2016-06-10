/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.content.Context;
import android.content.SharedPreferences;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.PrefsHelper.PrefHandler;

import java.lang.ref.WeakReference;

/**
 * Manages getting and setting any preferences related to telemetry.
 *
 * This class persists any Gecko preferences beyond shutdown so that these values
 * can be accessed on the next run before Gecko is started as we expect Telemetry
 * to run before Gecko is available.
 */
public class TelemetryPreferences {
    private TelemetryPreferences() {}

    private static final String GECKO_PREF_SERVER_URL = "toolkit.telemetry.server";
    private static final String SHARED_PREF_SERVER_URL = "telemetry-serverUrl";

    // Defaults are a mirror of about:config defaults so we can access them before Gecko is available.
    private static final String DEFAULT_SERVER_URL = "https://incoming.telemetry.mozilla.org";

    private static final String[] OBSERVED_PREFS = {
            GECKO_PREF_SERVER_URL,
    };

    public static String getServerSchemeHostPort(final Context context, final String profileName) {
        return getSharedPrefs(context, profileName).getString(SHARED_PREF_SERVER_URL, DEFAULT_SERVER_URL);
    }

    public static void initPreferenceObserver(final Context context, final String profileName) {
        final PrefHandler prefHandler = new TelemetryPrefHandler(context, profileName);
        PrefsHelper.addObserver(OBSERVED_PREFS, prefHandler); // gets preference value when gecko starts.
    }

    private static SharedPreferences getSharedPrefs(final Context context, final String profileName) {
        return GeckoSharedPrefs.forProfileName(context, profileName);
    }

    private static class TelemetryPrefHandler extends PrefsHelper.PrefHandlerBase {
        private final WeakReference<Context> contextWeakReference;
        private final String profileName;

        private TelemetryPrefHandler(final Context context, final String profileName) {
            contextWeakReference = new WeakReference<>(context);
            this.profileName = profileName;
        }

        @Override
        public void prefValue(final String pref, final String value) {
            final Context context = contextWeakReference.get();
            if (context == null) {
                return;
            }

            if (!pref.equals(GECKO_PREF_SERVER_URL)) {
                throw new IllegalStateException("Unknown preference: " + pref);
            }

            getSharedPrefs(context, profileName).edit()
                    .putString(SHARED_PREF_SERVER_URL, value)
                    .apply();
        }
    }
}
