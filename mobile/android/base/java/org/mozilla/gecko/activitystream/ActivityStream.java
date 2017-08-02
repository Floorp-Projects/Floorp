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
     * Is Activity Stream enabled?
     */
    public static boolean isEnabled(Context context) {
        // Fennec 57+ here we come!
        //   The only reason this method still exists is so that the old home panel code isn't
        //   suddenly unused and triggers all kinds of lint errors. However we should clean
        //   this up soon (Bug 1386725).
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
