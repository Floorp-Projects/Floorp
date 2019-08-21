/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;

import android.util.Log;
import android.text.TextUtils;

import org.mozilla.gecko.switchboard.Preferences;
import org.mozilla.gecko.switchboard.SwitchBoard;

import java.util.LinkedList;
import java.util.List;

/**
 * This class should reflect the experiment names found in the Switchboard experiments config here:
 * https://github.com/mozilla-services/switchboard-experiments
 */
public class Experiments {
    private static final String LOGTAG = "GeckoExperiments";

    // Enable the Enhanced Search Experiment.
    public static final String ENHANCED_SEARCH = "enhanced-search";

    // Show a system notification linking to a "What's New" page on app update.
    public static final String WHATSNEW_NOTIFICATION = "whatsnew-notification";

    // Synchronizing the catalog of downloadable content from Kinto
    public static final String DOWNLOAD_CONTENT_CATALOG_SYNC = "download-content-catalog-sync";

    // Promotion to bookmark reader-view items after entering reader view three times (Bug 1247689)
    public static final String TRIPLE_READERVIEW_BOOKMARK_PROMPT = "triple-readerview-bookmark-prompt";

    // Play HLS videos in a VideoView (Bug 1313391)
    public static final String HLS_VIDEO_PLAYBACK = "hls-video-playback";

    // Show AddOns menu-item in top level menu
    public static final String TOP_ADDONS_MENU = "top-addons-menu";

    // User in this group will enable Custom Tabs
    public static final String CUSTOM_TABS = "custom-tabs";

    // Enable full bookmark management(full-page dialog, bookmark/folder modification, etc.)
    public static final String FULL_BOOKMARK_MANAGEMENT = "full-bookmark-management";

    // Enable Leanplum SDK
    public static final String LEANPLUM = "leanplum-start";

    // Enable to use testing user id for Leanplum
    public static final String LEANPLUM_DEBUG = "leanplum-debug";

    // Enable processing of background telemetry.
    public static final String ENABLE_PROCESSING_BACKGROUND_TELEMETRY = "process-background-telemetry";

    /**
     * Returns list of all active experiments, remote and local.
     * @return List of experiment names Strings
     */
    public static List<String> getActiveExperiments(Context c) {
        final List<String> experiments = new LinkedList<>();
        experiments.addAll(SwitchBoard.getActiveExperiments(c));
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
