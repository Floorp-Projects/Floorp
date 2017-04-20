/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dawn;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;

import java.util.Locale;

/**
 * This helper class is specific for the termination of aurora release train, a.k.a. Project Dawn.
 * ( https://hacks.mozilla.org/2017/04/simplifying-firefox-release-channels/ )
 * We want to keep current aurora user transit to use nightly codebase with aurora package name on
 * Google play. The purpose is similar to org.mozilla.gecko.notifications.WhatsNewReceiver and org.mozilla.gecko.BrowserApp#conditionallyNotifyEOL.
 * TODO: Make this more generic and able to on/off from Switchboard.
 */
public class DawnHelper {

    private static final String ANDROID_PACKAGE_NAME = AppConstants.ANDROID_PACKAGE_NAME;
    private static final String ANDROID_PACKAGE_NAME_NIGHTLY_NEW = "org.mozilla.fennec_aurora";

    private static final String KEY_PERF_BOOLEAN_NOTIFY_DAWN_ENABLED = "key-pref-boolean-notify-dawn-enabled";
    private static final String TARGET_APP_VERSION = "55";


    private static boolean isAuroraNightly() {
        return ANDROID_PACKAGE_NAME.equals(ANDROID_PACKAGE_NAME_NIGHTLY_NEW);
    }

    private static boolean isTransitionRelease() {
        return AppConstants.MOZ_APP_VERSION.startsWith(TARGET_APP_VERSION);
    }

    public static boolean conditionallyNotifyDawn(@NonNull Context context) {
        if (!isAuroraNightly()) {
            return false;
        }

        if (!isTransitionRelease()) {
            return false;
        }

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
        if (!prefs.getBoolean(KEY_PERF_BOOLEAN_NOTIFY_DAWN_ENABLED, true)) {
            return false;
        } else {
            prefs.edit().putBoolean(KEY_PERF_BOOLEAN_NOTIFY_DAWN_ENABLED, false).apply();
        }

        final String link = context.getString(R.string.aurora_transition_notification_url,
                AppConstants.MOZ_APP_VERSION,
                AppConstants.OS_TARGET,
                Locales.getLanguageTag(Locale.getDefault()));

        Tabs.getInstance().loadUrl(link, Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED);

        return true;
    }

}
