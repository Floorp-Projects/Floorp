/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.utils.UrlUtils;

public class HomeScreen {
    private static final String BROADCAST_INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";
    public static final String ADD_TO_HOMESCREEN_TAG = "add_to_homescreen";

    /**
     * Create a shortcut for the given website on the device's home screen.
     */
    public static void installShortCut(Context context, Bitmap icon, String url, String title) {
        if (TextUtils.isEmpty(title.trim())) {
            title = generateTitleFromUrl(url);
        }

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            installShortCutViaBroadcast(context, icon, url, title);
        } else {
            installShortCutViaManager(context, icon, url, title);
        }
    }

    /**
     * Create a shortcut via system broadcast and then switch to the home screen.
     *
     * This works for Android versions up to 7.
     */
    private static void installShortCutViaBroadcast(Context context, Bitmap bitmap, String url, String title) {
        final Intent shortcutIntent = createShortcutIntent(context, url);

        final Intent addIntent = new Intent();
        addIntent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        addIntent.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        addIntent.putExtra(Intent.EXTRA_SHORTCUT_ICON, bitmap);
        addIntent.setAction(BROADCAST_INSTALL_SHORTCUT);

        context.sendBroadcast(addIntent);

        // Now let's switch to the home screen where the user will see the new shortcut appear.
        goToHomeScreen(context);
    }

    /**
     * Create a shortcut via the system's shortcut manager. This is the preferred way on Android 8+.
     *
     * The user will have the ability to add the shortcut manually or let the system place it automatically.
     */
    private static void installShortCutViaManager(Context context, Bitmap bitmap, String url, String title) {
        // TODO: For this we need to compile with Android SDK 26 (See issue #863)
    }

    private static Intent createShortcutIntent(Context context, String url) {
        final Intent shortcutIntent = new Intent(context, MainActivity.class);
        shortcutIntent.setAction(Intent.ACTION_VIEW);
        shortcutIntent.setData(Uri.parse(url));
        shortcutIntent.putExtra(ADD_TO_HOMESCREEN_TAG, ADD_TO_HOMESCREEN_TAG);
        return shortcutIntent;
    }

    @VisibleForTesting static String generateTitleFromUrl(String url) {
        // For now we just use the host name and strip common subdomains like "www" or "m".
        return UrlUtils.stripCommonSubdomains(Uri.parse(url).getHost());
    }

    /**
     * Switch to the the default home screen activity (launcher).
     */
    private static void goToHomeScreen(Context context) {
        Intent intent = new Intent(Intent.ACTION_MAIN);

        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}
