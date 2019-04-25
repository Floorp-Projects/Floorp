/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.res.Resources;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import java.util.Locale;

public class OnboardingStringUtil {
    private static OnboardingStringUtil INSTANCE;

    private static String initialWelcomeMessage;
    private static String updatedWelcomeMessage;
    private static String initialWelcomeSubtext =
            "A modern mobile browser from Mozilla, the non-profit committed to a free and open web.";
    private static String updatedWelcomeSubtext;

    private static String initialPrivacySubtext = "Private Browsing with Tracking Protection blocks " +
            "trackers while you browse and won’t remember your history when you finish browsing.";
    private static String updatedPrivacySubtext;

    private static String initialSyncSubtext = "Use Sync to find the bookmarks, passwords, and other " +
            "things you save to &brandShortName; on all your devices.";
    private static String updatedSyncSubtext;

    private static boolean areStringsLocalized = false;

    private OnboardingStringUtil(final Context context) {

        final Locale locale = context.getResources().getConfiguration().locale;
        if ("en".equals(locale.getLanguage())) {
            areStringsLocalized = true;
            return;
        }

        final Resources resources = context.getResources();
        initialWelcomeMessage = "Thanks for choosing " + AppConstants.MOZ_APP_BASENAME;
        updatedWelcomeMessage = resources.getString(R.string.firstrun_urlbar_message);
        updatedWelcomeSubtext = resources.getString(R.string.firstrun_urlbar_subtext);
        updatedPrivacySubtext = resources.getString(R.string.firstrun_privacy_subtext);
        updatedSyncSubtext = resources.getString(R.string.firstrun_sync_subtext);

        final boolean areWelcomeStringsLocalized =
                !initialWelcomeMessage.equals(updatedWelcomeMessage) &&
                !initialWelcomeSubtext.equals(updatedWelcomeSubtext);
        final boolean arePrivacyStringsLocalized =
                !initialPrivacySubtext.equals(updatedPrivacySubtext);
        final boolean areSyncStringsLocalized =
                !initialSyncSubtext.equals(updatedSyncSubtext);

        areStringsLocalized = areWelcomeStringsLocalized && arePrivacyStringsLocalized && areSyncStringsLocalized;
    }

    public static OnboardingStringUtil getInstance(final Context context) {
        if (INSTANCE == null) {
            INSTANCE = new OnboardingStringUtil(context);
        }
        return INSTANCE;
    }

    public boolean areStringsLocalized() {
        return areStringsLocalized;
    }
}
