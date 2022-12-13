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
import androidx.annotation.VisibleForTesting;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;
import android.text.TextUtils;

import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.utils.UrlUtils;

import java.util.UUID;

public class HomeScreen {
    public static final String ADD_TO_HOMESCREEN_TAG = "add_to_homescreen";
    public static final String BLOCKING_ENABLED = "blocking_enabled";
    public static final String REQUEST_DESKTOP = "request_desktop";

    /**
     * Create a shortcut for the given website on the device's home screen.
     */
    public static void installShortCut(Context context, Bitmap icon, String url, String title, boolean blockingEnabled, boolean requestDesktop) {
        if (TextUtils.isEmpty(title.trim())) {
            title = generateTitleFromUrl(url);
        }

        installShortCutViaManager(context, icon, url, title, blockingEnabled, requestDesktop);

        // Creating shortcut flow is different on Android up to 7, so we want to go
        // to the home screen manually where the user will see the new shortcut appear
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            goToHomeScreen(context);
        }
    }

    /**
     * Create a shortcut via the AppCompat's shortcut manager.
     * <p>
     * On Android versions up to 7 shortcut will be created via system broadcast internally.
     * <p>
     * On Android 8+ the user will have the ability to add the shortcut manually
     * or let the system place it automatically.
     */
    private static void installShortCutViaManager(Context context, Bitmap bitmap, String url, String title, boolean blockingEnabled, boolean requestDesktop) {
        if (ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            final IconCompat icon = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) ?
                    IconCompat.createWithAdaptiveBitmap(bitmap) : IconCompat.createWithBitmap(bitmap);
            final ShortcutInfoCompat shortcut = new ShortcutInfoCompat.Builder(context, UUID.randomUUID().toString())
                    .setShortLabel(title)
                    .setLongLabel(title)
                    .setIcon(icon)
                    .setIntent(createShortcutIntent(context, url, blockingEnabled, requestDesktop))
                    .build();
            ShortcutManagerCompat.requestPinShortcut(context, shortcut, null);
        }
    }

    private static Intent createShortcutIntent(Context context, String url, boolean blockingEnabled, boolean requestDesktop) {
        final Intent shortcutIntent = new Intent(context, MainActivity.class);
        shortcutIntent.setAction(Intent.ACTION_VIEW);
        shortcutIntent.setData(Uri.parse(url));
        shortcutIntent.putExtra(BLOCKING_ENABLED, blockingEnabled);
        shortcutIntent.putExtra(REQUEST_DESKTOP, requestDesktop);
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
