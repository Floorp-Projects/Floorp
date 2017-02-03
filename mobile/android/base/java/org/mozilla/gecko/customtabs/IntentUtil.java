/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.customtabs.CustomTabsIntent;

/**
 * A utility class for CustomTabsActivity to extract information from intent.
 * For example, this class helps to extract exit-animation resource id.
 */
class IntentUtil {

    public static final int NO_ANIMATION_RESOURCE = -1;

    // Hidden constant values from ActivityOptions.java
    private static final String PREFIX = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
            ? "android:activity."
            : "android:";
    private static final String KEY_PACKAGE_NAME = PREFIX + "packageName";
    private static final String KEY_ANIM_ENTER_RES_ID = PREFIX + "animEnterRes";
    private static final String KEY_ANIM_EXIT_RES_ID = PREFIX + "animExitRes";

    /**
     * To get package name of 3rd-party-app from an intent.
     * If the app defined extra exit-animation to use, it should also provide its package name
     * to get correct animation resource.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return package name, if the intent defined extra exit-animation bundle. Otherwise, null.
     */
    static String getAnimationPackageName(@NonNull Intent intent) {
        final Bundle bundle = getAnimationBundle(intent);
        return (bundle == null) ? null : bundle.getString(KEY_PACKAGE_NAME);
    }

    /**
     * To check whether the intent has necessary information to apply customize exit-animation.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return true, if the intent has necessary information.
     */
    static boolean hasExitAnimation(@NonNull Intent intent) {
        final Bundle bundle = getAnimationBundle(intent);
        return (bundle != null)
                && (getAnimationPackageName(intent) != null)
                && (getEnterAnimationRes(intent) != NO_ANIMATION_RESOURCE)
                && (getExitAnimationRes(intent) != NO_ANIMATION_RESOURCE);
    }

    /**
     * To get enter-animation resource id of 3rd-party-app from an intent.
     * If the app defined extra exit-animation to use, it should also provide its animation resource
     * id.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return animation resource id if any; otherwise, NO_ANIMATION_RESOURCE;
     */
    static int getEnterAnimationRes(@NonNull Intent intent) {
        final Bundle bundle = getAnimationBundle(intent);
        return (bundle == null)
                ? NO_ANIMATION_RESOURCE
                : bundle.getInt(KEY_ANIM_ENTER_RES_ID, NO_ANIMATION_RESOURCE);
    }

    /**
     * To get exit-animation resource id of 3rd-party-app from an intent.
     * If the app defined extra exit-animation to use, it should also provide its animation resource
     * id.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return animation resource id if any; otherwise, NO_ANIMATION_RESOURCE.
     */
    static int getExitAnimationRes(@NonNull Intent intent) {
        final Bundle bundle = getAnimationBundle(intent);
        return (bundle == null)
                ? NO_ANIMATION_RESOURCE
                : bundle.getInt(KEY_ANIM_EXIT_RES_ID, NO_ANIMATION_RESOURCE);
    }

    /**
     * To extract extra exit-animation bundle from an intent.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return bundle for extra exit-animation, if any. Otherwise, null.
     */
    private static Bundle getAnimationBundle(@NonNull Intent intent) {
        return intent.getBundleExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE);
    }
}
