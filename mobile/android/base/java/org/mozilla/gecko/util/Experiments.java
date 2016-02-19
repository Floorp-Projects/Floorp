/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.util.Log;
import org.mozilla.gecko.mozglue.ContextUtils.SafeIntent;

/**
 * This class should reflect the experiment names found in the Switchboard experiments config here:
 * https://github.com/mozilla-services/switchboard-experiments
 */
public class Experiments {
    private static final String LOGTAG = "GeckoExperiments";

    // Display History and Bookmarks in 3-dot menu.
    public static final String BOOKMARKS_HISTORY_MENU = "bookmark-history-menu";

    // Onboarding: "Features and Story"
    public static final String ONBOARDING2_A = "onboarding2-a"; // Control: Single (blue) welcome screen
    public static final String ONBOARDING2_B = "onboarding2-b"; // 4 static Feature slides
    public static final String ONBOARDING2_C = "onboarding2-c"; // 4 static + 1 clickable (Data saving) Feature slides

    // Show search mode (instead of home panels) when tapping on urlbar if there is a search term in the urlbar.
    public static final String SEARCH_TERM = "search-term";

    private static volatile Boolean disabled = null;

    /**
     * Determines whether Switchboard is disabled by the MOZ_DISABLE_SWITCHBOARD
     * environment variable. We need to read this value from the intent string
     * extra because environment variables from our test harness aren't set
     * until Gecko is loaded, and we need to know this before then.
     *
     * @param intent Main intent that launched the app
     * @return Whether Switchboard is disabled
     */
    public static boolean isDisabled(SafeIntent intent) {
        if (disabled != null) {
            return disabled;
        }

        String env = intent.getStringExtra("env0");
        for (int i = 1; env != null; i++) {
            if (env.startsWith("MOZ_DISABLE_SWITCHBOARD=")) {
                if (!env.endsWith("=")) {
                    Log.d(LOGTAG, "Switchboard disabled by MOZ_DISABLE_SWITCHBOARD environment variable");
                    disabled = true;
                    return disabled;
                }
            }
            env = intent.getStringExtra("env" + i);
        }
        disabled = false;
        return disabled;
    }
}
