/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;

/**
 * Provides information about the current environment.
 */
public class EnvironmentUtils {
    /**
     * Must be kept in-sync with {@link org.mozilla.gecko.GeckoApp#PREFS_IS_FIRST_RUN}.
     */
    private static final String GECKO_PREFS_IS_FIRST_RUN = "telemetry-isFirstRun";
    /**
     * Must be kept in-sync with {@link org.mozilla.gecko.firstrun.OnboardingHelper#FIRSTRUN_UUID}.
     */
    private static final String GECKO_PREFS_FIRSTRUN_UUID = "firstrun_uuid";

    public static boolean isFirstRun(final Context context) {
        return getDefaultGeckoSharedPreferences(context).getBoolean(GECKO_PREFS_IS_FIRST_RUN, true);
    }

    @Nullable
    public static String firstRunUUID(final Context context) {
        return getDefaultGeckoSharedPreferences(context).getString(GECKO_PREFS_FIRSTRUN_UUID, null);
    }

    // Note that this assumes our main browser activity (which owns this SharedPreference) is running
    // in the same process as callers of this code. Safely ccessing SharedPreferences from different
    // processes isn't well supported.
    private static SharedPreferences getDefaultGeckoSharedPreferences(final Context context) {
        // This direct access exists due to a limitation of the current build system. Services code
        // can't access non-services code directly.
        return context.getSharedPreferences("GeckoProfile-default", 0);
    }
}
