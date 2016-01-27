/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

/**
 * This class should reflect the experiment names found in the Switchboard experiments config here:
 * https://github.com/mozilla-services/switchboard-experiments
 */
public class Experiments {
    // Display History and Bookmarks in 3-dot menu.
    public static final String BOOKMARKS_HISTORY_MENU = "bookmark-history-menu";

    // Onboarding: "Features and Story"
    public static final String ONBOARDING2_A = "onboarding2-a"; // Control: Single (blue) welcome screen
    public static final String ONBOARDING2_B = "onboarding2-b"; // 4 static Feature slides
    public static final String ONBOARDING2_C = "onboarding2-c"; // 4 static + 1 clickable (Data saving) Feature slides

    // Show search mode (instead of home panels) when tapping on urlbar if there is a search term in the urlbar.
    public static final String SEARCH_TERM = "search-term";

}
