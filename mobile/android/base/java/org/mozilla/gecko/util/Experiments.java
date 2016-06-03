/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;

import android.util.Log;
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

    // Show a system notification linking to a "What's New" page on app update.
    public static final String WHATSNEW_NOTIFICATION = "whatsnew-notification";

    // Subscribe to known, bookmarked sites and show a notification if new content is available.
    public static final String CONTENT_NOTIFICATIONS_12HRS = "content-notifications-12hrs";
    public static final String CONTENT_NOTIFICATIONS_8AM = "content-notifications-8am";
    public static final String CONTENT_NOTIFICATIONS_5PM = "content-notifications-5pm";

    // Onboarding: "Features and Story". These experiments are determined
    // on the client, they are not part of the server config.
    public static final String ONBOARDING3_A = "onboarding3-a"; // Control: No first run
    public static final String ONBOARDING3_B = "onboarding3-b"; // 4 static Feature + 1 dynamic slides
    public static final String ONBOARDING3_C = "onboarding3-c"; // Differentiating features slides

    // Synchronizing the catalog of downloadable content from Kinto
    public static final String DOWNLOAD_CONTENT_CATALOG_SYNC = "download-content-catalog-sync";

    // Promotion for "Add to homescreen"
    public static final String PROMOTE_ADD_TO_HOMESCREEN = "promote-add-to-homescreen";

    public static final String PREF_ONBOARDING_VERSION = "onboarding_version";

    // Promotion to bookmark reader-view items after entering reader view three times (Bug 1247689)
    public static final String TRIPLE_READERVIEW_BOOKMARK_PROMPT = "triple-readerview-bookmark-prompt";

    // Only show origin in URL bar instead of full URL (Bug 1236431)
    public static final String URLBAR_SHOW_ORIGIN_ONLY = "urlbar-show-origin-only";

    // Show name of organization (EV cert) instead of full URL in URL bar (Bug 1249594).
    public static final String URLBAR_SHOW_EV_CERT_OWNER = "urlbar-show-ev-cert-owner";

    private static boolean isDisabled = false;

    public static void setIsDisabled(final boolean isDisabled) {
        Experiments.isDisabled = isDisabled;
        if (isDisabled) {
            Log.d(LOGTAG, "Switchboard disabled (env var?)");
        }
    }

    public static boolean isDisabled() {
        return isDisabled;
    }

    /**
     * Returns if a user is in certain local experiment.
     * @param experiment Name of experiment to look up
     * @return returns value for experiment or false if experiment does not exist.
     */
    public static boolean isInExperimentLocal(Context context, String experiment) {
        if (SwitchBoard.isInBucket(context, 0, 20)) {
            return Experiments.ONBOARDING3_A.equals(experiment);
        } else if (SwitchBoard.isInBucket(context, 20, 60)) {
            return Experiments.ONBOARDING3_B.equals(experiment);
        } else if (SwitchBoard.isInBucket(context, 60, 100)) {
            return Experiments.ONBOARDING3_C.equals(experiment);
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
