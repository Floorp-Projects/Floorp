/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;

/* package */ class TrackingProtection {
    private static final String TEST = "test-track-simple";
    private static final String AD = "ads-track-digest256";
    private static final String ANALYTIC = "analytics-track-digest256";
    private static final String SOCIAL = "social-track-digest256";
    private static final String CONTENT = "content-track-digest256";

    /* package */ static String buildPrefValue(int categories) {
        StringBuilder builder = new StringBuilder();

        if ((categories == TrackingProtectionDelegate.CATEGORY_NONE)) {
            return "";
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_TEST) != 0) {
            builder.append(TEST).append(',');
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_AD) != 0) {
            builder.append(AD).append(',');
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_ANALYTIC) != 0) {
            builder.append(ANALYTIC).append(',');
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_SOCIAL) != 0) {
            builder.append(SOCIAL).append(',');
        }
        if ((categories & TrackingProtectionDelegate.CATEGORY_CONTENT) != 0) {
            builder.append(CONTENT).append(',');
        }
        // Trim final ','.
        return builder.substring(0, builder.length() - 1);
    }

    /* package */ static int listToCategory(final String list) {
        int category = 0;
        if (list.indexOf(TEST) != -1) {
            category |= TrackingProtectionDelegate.CATEGORY_TEST;
        }
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
}
