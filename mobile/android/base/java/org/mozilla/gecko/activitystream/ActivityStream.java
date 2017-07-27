/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.AsyncTask;
import android.text.TextUtils;

import org.mozilla.gecko.switchboard.SwitchBoard;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.publicsuffix.PublicSuffix;

import java.util.Arrays;
import java.util.List;

public class ActivityStream {
    /**
     * List of undesired prefixes for labels based on a URL.
     *
     * This list is by no means complete and is based on those sources:
     * - https://gist.github.com/nchapman/36502ad115e8825d522a66549971a3f0
     * - https://github.com/mozilla/activity-stream/issues/1311
     */
    private static final List<String> UNDESIRED_LABEL_PREFIXES = Arrays.asList(
            "index.",
            "home."
    );

    /**
     * Undesired labels for labels based on a URL.
     *
     * This list is by no means complete and is based on those sources:
     * - https://gist.github.com/nchapman/36502ad115e8825d522a66549971a3f0
     * - https://github.com/mozilla/activity-stream/issues/1311
     */
    private static final List<String> UNDESIRED_LABELS = Arrays.asList(
            "render",
            "login",
            "edit"
    );

    /**
     * Returns true if the user has made an active decision: Enabling or disabling Activity Stream.
     */
    public static boolean hasUserEnabledOrDisabled(Context context) {
        final SharedPreferences preferences = GeckoSharedPrefs.forApp(context);
        return preferences.contains(GeckoPreferences.PREFS_ACTIVITY_STREAM);
    }

    /**
     * Set the user's decision: Enable or disable Activity Stream.
     */
    public static void setUserEnabled(Context context, boolean value) {
        GeckoSharedPrefs.forApp(context).edit()
                .putBoolean(GeckoPreferences.PREFS_ACTIVITY_STREAM, value)
                .apply();
    }

    /**
     * Returns true if Activity Stream has been enabled by the user. Before calling this method
     * hasUserEnabledOrDisabled() should be used to determine whether the user actually has made
     * a decision.
     */
    public static boolean isEnabledByUser(Context context) {
        final SharedPreferences preferences = GeckoSharedPrefs.forApp(context);
        if (!preferences.contains(GeckoPreferences.PREFS_ACTIVITY_STREAM)) {
            throw new IllegalStateException("User hasn't made a decision. Call hasUserEnabledOrDisabled() before calling this method");
        }

        return preferences.getBoolean(GeckoPreferences.PREFS_ACTIVITY_STREAM, /* should not be used */ false);
    }

    /**
     * Is Activity Stream enabled by an A/B experiment?
     */
    public static boolean isEnabledByExperiment(Context context) {
        // For users in the "opt out" group Activity Stream is enabled by default.
        return SwitchBoard.isInExperiment(context, Experiments.ACTIVITY_STREAM_OPT_OUT);
    }

    /**
     * Is Activity Stream enabled? Either actively by the user or by an experiment?
     */
    public static boolean isEnabled(Context context) {
        // (1) Can Activity Steam be enabled on this device?
        if (!canBeEnabled(context)) {
            return false;
        }

        // (2) Has Activity Stream be enabled/disabled by the user?
        if (hasUserEnabledOrDisabled(context)) {
            return isEnabledByUser(context);
        }

        // (3) Is Activity Stream enabled by an experiment?
        return isEnabledByExperiment(context);
    }

    /**
     * Can the user enable/disable Activity Stream (Returns true) or is this completely controlled by us?
     */
    public static boolean isUserSwitchable(Context context) {
        // (1) Can Activity Steam be enabled on this device?
        if (!canBeEnabled(context)) {
            return false;
        }

        // (2) Is the user part of the experiment for showing the settings UI?
        return SwitchBoard.isInExperiment(context, Experiments.ACTIVITY_STREAM_SETTING);
    }

    /**
     * This method returns true if Activity Stream can be enabled - by the user or an experiment.
     * Whether a setting shows up or whether the user is in an experiment group is evaluated
     * separately from this method. However if this methods returns false then Activity Stream
     * should never be visible/enabled - no matter what build or what experiments are active.
     */
    public static boolean canBeEnabled(Context context) {
        if (!AppConstants.NIGHTLY_BUILD) {
            // If this is not a Nightly build then hide Activity Stream completely. We can control
            // this via the Switchboard experiment too but I want to make really sure that this
            // isn't riding the trains accidentally.
            return false;
        }

        if (!SwitchBoard.isInExperiment(context, Experiments.ACTIVITY_STREAM)) {
            // This is our kill switch. If the user is not part of this experiment then show no
            // Activity Stream UI.
            return false;
        }

        // Activity stream can be enabled. Whether it is depends on other experiments and settings.
        return true;
    }

    /**
     * Query whether we want to display Activity Stream as a Home Panel (within the HomePager),
     * or as a HomePager replacement.
     */
    public static boolean isHomePanel() {
        return true;
    }
}
