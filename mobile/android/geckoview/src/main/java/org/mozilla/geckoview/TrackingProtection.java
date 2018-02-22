/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;

import org.mozilla.gecko.PrefsHelper;
import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;

/* package */ class TrackingProtection {
    private static final boolean DEBUG = false;

    private static final String LOGTAG = "GeckoViewTrackingProtection";
    private static final String TRACKERS_PREF = "urlclassifier.trackingTable";
    private static final String LISTS_PREF =
        "browser.safebrowsing.provider.mozilla.lists";

    private static final String AD = "ads-track-digest256";
    private static final String ANALYTIC = "analytics-track-digest256";
    private static final String SOCIAL = "social-track-digest256";
    private static final String CONTENT = "content-track-digest256";

    private String buildPrefValue(int categories) {
        StringBuilder builder = new StringBuilder();
        builder.append("test-track-simple");

        if ((categories & TrackingProtectionDelegate.CATEGORY_AD) ==
            TrackingProtectionDelegate.CATEGORY_AD) {
            builder.append(",").append(AD);
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_ANALYTIC) ==
            TrackingProtectionDelegate.CATEGORY_ANALYTIC) {
            builder.append(",").append(ANALYTIC);
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_SOCIAL) ==
            TrackingProtectionDelegate.CATEGORY_SOCIAL) {
            builder.append(",").append(SOCIAL);
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_CONTENT) ==
            TrackingProtectionDelegate.CATEGORY_CONTENT) {
            builder.append(",").append(CONTENT);
        }
        return builder.toString();
    }

    /* package */ static int listToCategory(final String list) {
        int category = 0;
        if (list.indexOf(AD) != -1) {
            category |= TrackingProtectionDelegate.CATEGORY_AD;
        }
        if (list.indexOf(ANALYTIC) != -1) {
            category |= TrackingProtectionDelegate.CATEGORY_ANALYTIC;
        }
        if (list.indexOf(SOCIAL) != -1) {
            category |= TrackingProtectionDelegate.CATEGORY_SOCIAL;
        }
        if (list.indexOf(CONTENT) != -1) {
            category |= TrackingProtectionDelegate.CATEGORY_CONTENT;
        }
        return category;
    }

    private final GeckoSession mSession;

    /* package */ TrackingProtection(final GeckoSession session) {
        mSession = session;
    }

    public void enable(final int categories) {
        if (categories == 0) {
            disable();
            return;
        }

        final String prefValue = buildPrefValue(categories);
        PrefsHelper.setPref(TRACKERS_PREF, prefValue);

        mSession.getSettings().setBoolean(
            GeckoSessionSettings.USE_TRACKING_PROTECTION, true);

        if (DEBUG) Log.d(LOGTAG, "Tracking protection enabled (" + prefValue + ")");
    }

    public void disable() {
        mSession.getSettings().setBoolean(
            GeckoSessionSettings.USE_TRACKING_PROTECTION, false);
    }
}
