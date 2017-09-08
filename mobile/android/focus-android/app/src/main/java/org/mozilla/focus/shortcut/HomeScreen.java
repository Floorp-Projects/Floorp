/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.utils.UrlUtils;

public class HomeScreen {
    private static final String BROADCAST_INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";
    public static final String ADD_TO_HOMESCREEN_TAG = "add_to_homescreen";
    public static final String BLOCKING_ENABLED = "blocking_enabled";

    /**
     * Create a shortcut for the given website on the device's home screen.
     */
    public static void installShortCut(Context context, Bitmap icon, String url, String title, boolean blockingEnabled) {
        if (TextUtils.isEmpty(title.trim())) {
            title = generateTitleFromUrl(url);
        }
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            installShortCutViaBroadcast(context, icon, url, title, blockingEnabled);
        } else {
            installShortCutViaManager(context, icon, url, title, blockingEnabled);
        }
    }

    /**
     * Create a shortcut via system broadcast and then switch to the home screen.
     *
     * This works for Android versions up to 7.
     */
    private static void installShortCutViaBroadcast(Context context, Bitmap bitmap, String url, String title, boolean blockingEnabled) {
        final Intent shortcutIntent = createShortcutIntent(context, url, blockingEnabled);

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
    @TargetApi(26)
    private static void installShortCutViaManager(Context context, Bitmap bitmap, String url, String title, boolean blockingEnabled) {
        final ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
        if (shortcutManager == null) {
            return;
        }

        if (shortcutManager.isRequestPinShortcutSupported()) {
            final ShortcutInfo shortcut = new ShortcutInfo.Builder(context, url)
                    .setShortLabel(title)
                    .setLongLabel(title)
                    .setIcon(Icon.createWithBitmap(bitmap))
                    .setIntent(createShortcutIntent(context, url, blockingEnabled))
                    .build();
            shortcutManager.requestPinShortcut(shortcut, null);
        }
    }

    private static Intent createShortcutIntent(Context context, String url, boolean blockingEnabled) {
        final Intent shortcutIntent = new Intent(context, MainActivity.class);
        shortcutIntent.setAction(Intent.ACTION_VIEW);
        shortcutIntent.setData(Uri.parse(url));
        shortcutIntent.putExtra(BLOCKING_ENABLED, blockingEnabled);
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
