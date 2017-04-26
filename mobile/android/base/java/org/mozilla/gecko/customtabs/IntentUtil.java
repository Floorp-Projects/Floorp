/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.support.customtabs.CustomTabsIntent;

import java.util.ArrayList;
import java.util.List;

/**
 * A utility class for CustomTabsActivity to extract information from intent.
 * For example, this class helps to extract exit-animation resource id.
 */
class IntentUtil {

    public static final int NO_ANIMATION_RESOURCE = -1;

    @VisibleForTesting
    @ColorInt
    protected static final int DEFAULT_ACTION_BAR_COLOR = 0xFF363b40; // default color to match design

    // Hidden constant values from ActivityOptions.java
    private static final String PREFIX = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
            ? "android:activity."
            : "android:";
    private static final String KEY_PACKAGE_NAME = PREFIX + "packageName";
    private static final String KEY_ANIM_ENTER_RES_ID = PREFIX + "animEnterRes";
    private static final String KEY_ANIM_EXIT_RES_ID = PREFIX + "animExitRes";

    /**
     * To determine whether the intent has necessary information to build an Action-Button.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return true, if intent has all necessary information.
     */
    static boolean hasActionButton(@NonNull Intent intent) {
        return (getActionButtonBundle(intent) != null)
                && (getActionButtonIcon(intent) != null)
                && (getActionButtonDescription(intent) != null)
                && (getActionButtonPendingIntent(intent) != null);
    }

    /**
     * To determine whether the intent requires to add share action to menu item.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return true, if intent requires to add share action to menu item.
     */
    static boolean hasShareItem(@NonNull Intent intent) {
        return intent.getBooleanExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, false);
    }

    /**
     * To extract bitmap icon from intent for Action-Button.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return bitmap icon, if any. Otherwise, null.
     */
    static Bitmap getActionButtonIcon(@NonNull Intent intent) {
        final Bundle bundle = getActionButtonBundle(intent);
        return (bundle == null) ? null : (Bitmap) bundle.getParcelable(CustomTabsIntent.KEY_ICON);
    }

    /**
     * Only for telemetry to understand caller app's customization
     * This method should only be called once during one usage.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return true, if the caller customized the color.
     */
    static boolean hasToolbarColor(@NonNull Intent intent) {
        return intent.hasExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR);
    }

    /**
     * To extract color code from intent for top toolbar.
     * It also ensure the color is not translucent.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return color code in integer type.
     */
    @ColorInt
    static int getToolbarColor(@NonNull Intent intent) {
        @ColorInt int toolbarColor = intent.getIntExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR,
                DEFAULT_ACTION_BAR_COLOR);

        // Translucent color does not make sense for toolbar color. Ensure it is 0xFF.
        toolbarColor = 0xFF000000 | toolbarColor;
        return toolbarColor;
    }

    /**
     * To extract description from intent for Action-Button. This description is used for
     * accessibility.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return description, if any. Otherwise, null.
     */
    static String getActionButtonDescription(@NonNull Intent intent) {
        final Bundle bundle = getActionButtonBundle(intent);
        return (bundle == null) ? null : bundle.getString(CustomTabsIntent.KEY_DESCRIPTION);
    }

    /**
     * To extract pending-intent from intent for Action-Button.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return PendingIntent, if any. Otherwise, null.
     */
    static PendingIntent getActionButtonPendingIntent(@NonNull Intent intent) {
        final Bundle bundle = getActionButtonBundle(intent);
        return (bundle == null)
                ? null
                : (PendingIntent) bundle.getParcelable(CustomTabsIntent.KEY_PENDING_INTENT);
    }

    /**
     * To know whether the Action-Button should be tinted.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return true, if Action-Button should be tinted. Default value is false.
     */
    static boolean isActionButtonTinted(@NonNull Intent intent) {
        return intent.getBooleanExtra(CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON, false);
    }

    /**
     * To extract extra Action-button bundle from an intent.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return bundle for Action-Button, if any. Otherwise, null.
     */
    private static Bundle getActionButtonBundle(@NonNull Intent intent) {
        return intent.getBundleExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE);
    }

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
     * To get titles for Menu Items from an intent.
     * 3rd-party-app is able to add and customize up to five menu items. This method helps to
     * get titles for each menu items.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return A list of string as title for each menu items
     */
    static List<String> getMenuItemsTitle(@NonNull Intent intent) {
        final List<Bundle> bundles = getMenuItemsBundle(intent);
        final List<String> titles = new ArrayList<>();
        for (Bundle b : bundles) {
            titles.add(b.getString(CustomTabsIntent.KEY_MENU_ITEM_TITLE));
        }
        return titles;
    }

    /**
     * To get pending-intent for Menu Items from an intent.
     * 3rd-party-app is able to add and customize up to five menu items. This method helps to
     * get pending-intent for each menu items.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return A list of pending-intent for each menu items
     */
    static List<PendingIntent> getMenuItemsPendingIntent(@NonNull Intent intent) {
        final List<Bundle> bundles = getMenuItemsBundle(intent);
        final List<PendingIntent> intents = new ArrayList<>();
        for (Bundle b : bundles) {
            PendingIntent p = b.getParcelable(CustomTabsIntent.KEY_PENDING_INTENT);
            intents.add(p);
        }
        return intents;
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

    /**
     * To extract bundles for Menu Items from an intent.
     * 3rd-party-app is able to add and customize up to five menu items. This method helps to
     * extract bundles to build menu items.
     *
     * @param intent which to launch a Custom-Tabs-Activity
     * @return bundle for menu items, if any. Otherwise, an empty list.
     */
    private static List<Bundle> getMenuItemsBundle(@NonNull Intent intent) {
        ArrayList<Bundle> extra = intent.getParcelableArrayListExtra(
                CustomTabsIntent.EXTRA_MENU_ITEMS);
        return (extra == null) ? new ArrayList<Bundle>() : extra;
    }
}
