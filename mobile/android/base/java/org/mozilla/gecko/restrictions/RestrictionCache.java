/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.UserManager;

import org.mozilla.gecko.util.ThreadUtils;

/**
 * Cache for user and application restrictions.
 */
public class RestrictionCache {
    private static Bundle cachedAppRestrictions;
    private static Bundle cachedUserRestrictions;
    private static boolean isCacheInvalid = true;

    private RestrictionCache() {}

    public static synchronized boolean getUserRestriction(Context context, String restriction) {
        updateCacheIfNeeded(context);
        return cachedUserRestrictions.getBoolean(restriction);
    }

    public static synchronized boolean hasApplicationRestriction(Context context, String restriction) {
        updateCacheIfNeeded(context);
        return cachedAppRestrictions.containsKey(restriction);
    }

    public static synchronized boolean getApplicationRestriction(Context context, String restriction, boolean defaultValue) {
        updateCacheIfNeeded(context);
        return cachedAppRestrictions.getBoolean(restriction, defaultValue);
    }

    public static synchronized boolean hasApplicationRestrictions(Context context) {
        updateCacheIfNeeded(context);
        return !cachedAppRestrictions.isEmpty();
    }

    public static synchronized void invalidate() {
        isCacheInvalid = true;
    }

    private static void updateCacheIfNeeded(Context context) {
        // If we are not on the UI thread then we can just go ahead and read the values (Bug 1189347).
        // Otherwise we read from the cache to avoid blocking the UI thread. If the cache is invalid
        // then we hazard the consequences and just do the read.
        if (isCacheInvalid || !ThreadUtils.isOnUiThread()) {
            readRestrictions(context);
            isCacheInvalid = false;
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static void readRestrictions(Context context) {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);

        // If we do not have anything in the cache yet then this read might happen on the UI thread (Bug 1189347).
        final StrictMode.ThreadPolicy policy = StrictMode.allowThreadDiskReads();

        try {
            Bundle appRestrictions = mgr.getApplicationRestrictions(context.getPackageName());
            migrateRestrictionsIfNeeded(appRestrictions);

            cachedAppRestrictions = appRestrictions;
            cachedUserRestrictions = mgr.getUserRestrictions(); // Always implies disk read
        } finally {
            StrictMode.setThreadPolicy(policy);
        }
    }

    /**
     * This method migrates the old set of DISALLOW_ restrictions to the new restrictable feature ones (Bug 1189336).
     */
    /* package-private */ static void migrateRestrictionsIfNeeded(Bundle bundle) {
        if (!bundle.containsKey(Restrictable.INSTALL_EXTENSION.name) && bundle.containsKey("no_install_extensions")) {
            bundle.putBoolean(Restrictable.INSTALL_EXTENSION.name, !bundle.getBoolean("no_install_extensions"));
        }

        if (!bundle.containsKey(Restrictable.PRIVATE_BROWSING.name) && bundle.containsKey("no_private_browsing")) {
            bundle.putBoolean(Restrictable.PRIVATE_BROWSING.name, !bundle.getBoolean("no_private_browsing"));
        }

        if (!bundle.containsKey(Restrictable.CLEAR_HISTORY.name) && bundle.containsKey("no_clear_history")) {
            bundle.putBoolean(Restrictable.CLEAR_HISTORY.name, !bundle.getBoolean("no_clear_history"));
        }

        if (!bundle.containsKey(Restrictable.ADVANCED_SETTINGS.name) && bundle.containsKey("no_advanced_settings")) {
            bundle.putBoolean(Restrictable.ADVANCED_SETTINGS.name, !bundle.getBoolean("no_advanced_settings"));
        }
    }
}
