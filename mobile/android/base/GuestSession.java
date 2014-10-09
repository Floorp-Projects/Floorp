/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko;

import android.app.KeyguardManager;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.support.v4.app.NotificationCompat;
import android.view.Window;
import android.view.WindowManager;

// Utility methods for entering/exiting guest mode.
public class GuestSession {
    public static final String NOTIFICATION_INTENT = "org.mozilla.gecko.GUEST_SESSION_INPROGRESS";
    private static final String LOGTAG = "GeckoGuestSession";

    // Returns true if the user is using a secure keyguard, and its currently locked.
    static boolean isSecureKeyguardLocked(Context context) {
        final KeyguardManager manager = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);

        // The test machines return null for the KeyguardService, despite running Android 4.2.
        if (AppConstants.Versions.preJB || manager == null) {
            return false;
        }

        return manager.isKeyguardLocked() && manager.isKeyguardSecure();
    }

    /* Returns true if you should be in guest mode. This can be because a secure keyguard
     * is locked, or because the user has explicitly started guest mode via a dialog. If the
     * user has explicitly started Fennec in guest mode, this will return true until they
     * explicitly exit it.
     */
    public static boolean shouldUse(final Context context, final String args) {
        // Did the command line args request guest mode?
        if (args != null && args.contains(BrowserApp.GUEST_BROWSING_ARG)) {
            return true;
        }

        // Otherwise, is the device locked?
        final boolean keyguard = isSecureKeyguardLocked(context);
        if (keyguard) {
            return true;
        }

        // Otherwise, is there a locked guest mode profile?
        final GeckoProfile profile = GeckoProfile.getGuestProfile(context);
        if (profile == null) {
            return false;
        }

        return profile.locked();
    }

    public static void configureWindow(Window window) {
        // In guest sessions we allow showing over the keyguard.
        window.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    }

    public static void unconfigureWindow(Window window) {
        // In guest sessions we allow showing over the keyguard.
        window.clearFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    }

    private static PendingIntent getNotificationIntent(Context context) {
        Intent intent = new Intent(NOTIFICATION_INTENT);
        intent.setClass(context, BrowserApp.class);
        return PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    public static void showNotification(Context context) {
        final NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
        final Resources res = context.getResources();
        builder.setContentTitle(res.getString(R.string.guest_browsing_notification_title))
               .setContentText(res.getString(R.string.guest_browsing_notification_text))
               .setSmallIcon(R.drawable.alert_guest)
               .setOngoing(true)
               .setContentIntent(getNotificationIntent(context));

        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(R.id.guestNotification, builder.build());
    }

    public static void hideNotification(Context context) {
        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(R.id.guestNotification);
    }

    public static void handleIntent(BrowserApp context, Intent intent) {
        context.showGuestModeDialog(BrowserApp.GuestModeDialog.LEAVING);
    }

}
