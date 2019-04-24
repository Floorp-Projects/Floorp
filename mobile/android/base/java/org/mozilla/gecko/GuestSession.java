/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.mozilla.gecko.notifications.NotificationHelper;

// Utility methods for entering/exiting guest mode.
public final class GuestSession {
    private static final String LOGTAG = "GeckoGuestSession";

    public static final String NOTIFICATION_INTENT = "org.mozilla.gecko.GUEST_SESSION_INPROGRESS";

    private static PendingIntent getNotificationIntent(Context context) {
        Intent intent = new Intent(NOTIFICATION_INTENT);
        intent.setClassName(context, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        return PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @SuppressWarnings("NewApi")
    public static void showNotification(Context context) {
        final Notification.Builder builder = new Notification.Builder(context);
        final Resources res = context.getResources();
        builder.setContentTitle(res.getString(R.string.guest_browsing_notification_title))
               .setContentText(res.getString(R.string.guest_browsing_notification_text))
               .setSmallIcon(R.drawable.alert_guest)
               .setOngoing(true)
               .setContentIntent(getNotificationIntent(context));

        if (!AppConstants.Versions.preO) {
            builder.setChannelId(NotificationHelper.getInstance(context)
                    .getNotificationChannel(NotificationHelper.Channel.DEFAULT).getId());
        }

        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(R.id.guestNotification, builder.build());
    }

    public static void hideNotification(Context context) {
        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(R.id.guestNotification);
    }

    public static void onNotificationIntentReceived(BrowserApp context) {
        context.showGuestModeDialog(BrowserApp.GuestModeDialog.LEAVING);
    }

}
