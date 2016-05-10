/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;

import android.util.Log;
import org.mozilla.gecko.mozglue.ContextUtils.SafeIntent;
import android.text.TextUtils;

import com.keepsafe.switchboard.Preferences;
import com.keepsafe.switchboard.SwitchBoard;
import org.mozilla.gecko.GeckoSharedPrefs;

import java.util.LinkedList;
import java.util.List;

/**
 * This class should reflect the experiment names found in the Switchboard experiments config here:
 * https://github.com/mozilla-services/switchboard-experiments
 */
public class Experiments {
    private static final String LOGTAG = "GeckoExperiments";

    // Show search mode (instead of home panels) when tapping on urlbar if there is a search term in the urlbar.
    public static final String SEARCH_TERM = "search-term";

    // Show a system notification linking to a "What's New" page on app update.
    public static final String WHATSNEW_NOTIFICATION = "whatsnew-notification";

    // Subscribe to known, bookmarked sites and show a notification if new content is available.
    public static final String CONTENT_NOTIFICATIONS_12HRS = "content-notifications-12hrs";
    public static final String CONTENT_NOTIFICATIONS_8AM = "content-notifications-8am";
    public static final String CONTENT_NOTIFICATIONS_5PM = "content-notifications-5pm";

    // Onboarding: "Features and Story". These experiments are determined
    // on the client, they are not part of the server config.
    public static final String ONBOARDING2_A = "onboarding2-a"; // Control: Single (blue) welcome screen
    public static final String ONBOARDING2_B = "onboarding2-b"; // 4 static Feature slides
    public static final String ONBOARDING2_C = "onboarding2-c"; // 4 static + 1 clickable (Data saving) Feature slides

    // Synchronizing the catalog of downloadable content from Kinto
    public static final String DOWNLOAD_CONTENT_CATALOG_SYNC = "download-content-catalog-sync";

    // Promotion for "Add to homescreen"
    public static final String PROMOTE_ADD_TO_HOMESCREEN = "promote-add-to-homescreen";

    public static final String PREF_ONBOARDING_VERSION = "onboarding_version";

    // Promotion to bookmark reader-view items after entering reader view three times (Bug 1247689)
    public static final String TRIPLE_READERVIEW_BOOKMARK_PROMPT = "triple-readerview-bookmark-prompt";

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

    /**
     * Returns if a user is in certain local experiment.
     * @param experiment Name of experiment to look up
     * @return returns value for experiment or false if experiment does not exist.
     */
    public static boolean isInExperimentLocal(Context context, String experiment) {
        if (SwitchBoard.isInBucket(context, 0, 33)) {
            return Experiments.ONBOARDING2_A.equals(experiment);
        } else if (SwitchBoard.isInBucket(context, 33, 66)) {
            return Experiments.ONBOARDING2_B.equals(experiment);
        } else if (SwitchBoard.isInBucket(context, 66, 100)) {
            return Experiments.ONBOARDING2_C.equals(experiment);
        } else {
            return false;
        }
    }

    /**
     * Returns list of all active experiments, remote and local.
     * @return List of experiment names Strings
     */
    public static List<String> getActiveExperiments(Context c) {
        final List<String> experiments = new LinkedList<>();
        experiments.addAll(SwitchBoard.getActiveExperiments(c));

        // Add onboarding version.
        final String onboardingExperiment = GeckoSharedPrefs.forProfile(c).getString(Experiments.PREF_ONBOARDING_VERSION, null);
        if (!TextUtils.isEmpty(onboardingExperiment)) {
            experiments.add(onboardingExperiment);
        }

        return experiments;
    }

    /**
     * Sets an override to force an experiment to be enabled or disabled. This value
     * will be read and used before reading the switchboard server configuration.
     *
     * @param c Context
     * @param experimentName Experiment name
     * @param isEnabled Whether or not the experiment should be enabled
     */
    public static void setOverride(Context c, String experimentName, boolean isEnabled) {
        Log.d(LOGTAG, "setOverride: " + experimentName + " = " + isEnabled);
        Preferences.setOverrideValue(c, experimentName, isEnabled);
    }

    /**
     * Clears the override value for an experiment.
     *
     * @param c Context
     * @param experimentName Experiment name
     */
    public static void clearOverride(Context c, String experimentName) {
        Log.d(LOGTAG, "clearOverride: " + experimentName);
        Preferences.clearOverrideValue(c, experimentName);
    }
}
