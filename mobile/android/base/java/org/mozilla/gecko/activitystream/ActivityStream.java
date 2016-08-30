/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.content.Context;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;

public class ActivityStream {
    public static boolean isEnabled(Context context) {
        if (!AppConstants.MOZ_ANDROID_ACTIVITY_STREAM) {
            return false;
        }

        return GeckoSharedPrefs.forApp(context)
                .getBoolean(GeckoPreferences.PREFS_ACTIVITY_STREAM, false);
    }

    /**
     * Query whether we want to display Activity Stream as a Home Panel (within the HomePager),
     * or as a HomePager replacement.
     */
    public static boolean isHomePanel() {
        return true;
    }
}
