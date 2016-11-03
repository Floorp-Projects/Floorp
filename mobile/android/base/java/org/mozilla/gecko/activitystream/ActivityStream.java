/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream;

import android.content.Context;
import android.net.Uri;
import android.os.AsyncTask;
import android.text.TextUtils;

import com.keepsafe.switchboard.SwitchBoard;

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

    public static boolean isEnabled(Context context) {
        if (!isUserEligible(context)) {
            // If the user is not eligible then disable activity stream. Even if it has been
            //  enabled before.
            return false;
        }

        return GeckoSharedPrefs.forApp(context)
                .getBoolean(GeckoPreferences.PREFS_ACTIVITY_STREAM, false);
    }

    /**
     * Is the user eligible to use activity stream or should we hide it from settings etc.?
     */
    public static boolean isUserEligible(Context context) {
        if (AppConstants.MOZ_ANDROID_ACTIVITY_STREAM) {
            // If the build flag is enabled then just show the option to the user.
            return true;
        }

        if (AppConstants.NIGHTLY_BUILD && SwitchBoard.isInExperiment(context, Experiments.ACTIVITY_STREAM)) {
            // If this is a nightly build and the user is part of the activity stream experiment then
            // the option should be visible too. The experiment is limited to Nightly too but I want
            // to make really sure that this isn't riding the trains accidentally.
            return true;
        }

        // For everyone else activity stream is not available yet.
        return false;
    }

    /**
     * Query whether we want to display Activity Stream as a Home Panel (within the HomePager),
     * or as a HomePager replacement.
     */
    public static boolean isHomePanel() {
        return true;
    }

    /**
     * Extract a label from a URL to use in Activity Stream.
     *
     * This method implements the proposal from this desktop AS issue:
     * https://github.com/mozilla/activity-stream/issues/1311
     *
     * @param usePath Use the path of the URL to extract a label (if suitable)
     */
    public static void extractLabel(final Context context, final String url, final boolean usePath, final LabelCallback callback) {
        new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... params) {
                if (TextUtils.isEmpty(url)) {
                    return "";
                }

                final Uri uri = Uri.parse(url);

                // Use last path segment if suitable
                if (usePath) {
                    final String segment = uri.getLastPathSegment();
                    if (!TextUtils.isEmpty(segment)
                            && !UNDESIRED_LABELS.contains(segment)
                            && !segment.matches("^[0-9]+$")) {

                        boolean hasUndesiredPrefix = false;
                        for (int i = 0; i < UNDESIRED_LABEL_PREFIXES.size(); i++) {
                            if (segment.startsWith(UNDESIRED_LABEL_PREFIXES.get(i))) {
                                hasUndesiredPrefix = true;
                                break;
                            }
                        }

                        if (!hasUndesiredPrefix) {
                            return segment;
                        }
                    }
                }

                // If no usable path segment was found then use the host without public suffix and common subdomains
                final String host = uri.getHost();
                if (TextUtils.isEmpty(host)) {
                    return url;
                }

                return StringUtils.stripCommonSubdomains(
                        PublicSuffix.stripPublicSuffix(context, host));
            }

            @Override
            protected void onPostExecute(String label) {
                callback.onLabelExtracted(label);
            }
        }.execute();
    }

    public abstract static class LabelCallback {
        public abstract void onLabelExtracted(String label);
    }
}
