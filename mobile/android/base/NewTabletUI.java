/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.SharedPreferences;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.HardwareUtils;

public class NewTabletUI {
    // This value should be in sync with preferences_display.xml. On non-release
    // builds, the preference UI will be hidden and the (unused) default
    // preference UI value will still be 'true'.
    private static final boolean DEFAULT = !AppConstants.RELEASE_BUILD;

    private static Boolean sNewTabletUI;

    public static synchronized boolean isEnabled(Context context) {
        if (!HardwareUtils.isTablet()) {
            return false;
        }

        if (sNewTabletUI == null) {
            final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
            sNewTabletUI = prefs.getBoolean(GeckoPreferences.PREFS_NEW_TABLET_UI, DEFAULT);
        }

        return sNewTabletUI;
    }
}