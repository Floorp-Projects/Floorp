/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Activity;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

import java.util.HashMap;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoActivityMonitor;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoService;
import org.mozilla.gecko.NotificationListener;
import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;

/**
 * Client for posting notifications.
 */
public final class NotificationClient implements NotificationListener {
    private static final String LOGTAG = "GeckoNotificationClient";
    /* package */ static final String CLICK_ACTION = AppConstants.ANDROID_PACKAGE_NAME + ".NOTIFICATION_CLICK";
    /* package */ static final String CLOSE_ACTION = AppConstants.ANDROID_PACKAGE_NAME + ".NOTIFICATION_CLOSE";
    /* package */ static final String PERSISTENT_INTENT_EXTRA = "persistentIntent";

    private final Context mContext;
    private final NotificationManagerCompat mNotificationManager;

    private final HashMap<String, Notification> mNotifications = new HashMap<>();

    /**
     * Notification associated with this service's foreground state.
     *
     * {@link android.app.Service#startForeground(int, android.app.Notification)}
     * associates the foreground with exactly one notification from the service.
     * To keep Fennec alive during downloads (and to make sure it can be killed
     * once downloads are complete), we make sure that the foreground is always
     * associated with an active progress notification if and only if at least
     * one download is in progress.
     */
    private String mForegroundNotification;

    public NotificationClient(Context context) {
        mContext = context.getApplicationContext();
        mNotificationManager = NotificationManagerCompat.from(mContext);
    }

    @Override // NotificationListener
    public void showNotification(String name, String cookie, String title,
                                 String text, String host, String imageUrl) {
        showNotification(name, cookie, title, text, host, imageUrl, /* data */ null);
    }

    @Override // NotificationListener
    public void showPersistentNotification(String name, String cookie, String title,
                                           String text, String host, String imageUrl,
                                           String data) {
        showNotification(name, cookie, title, text, host, imageUrl, data != null ? data : "");
    }

    private void showNotification(String name, String cookie, String title,
                                  String text, String host, String imageUrl,
                                  String persistentData) {
        // Put the strings into the intent as an URI
        // "alert:?name=<name>&cookie=<cookie>"
        String packageName = mContext.getPackageName();
        String className = AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS;

        final Activity currentActivity =
                GeckoActivityMonitor.getInstance().getCurrentActivity();
        if (currentActivity != null) {
            className = currentActivity.getClass().getName();
        }
        final Uri dataUri = (new Uri.Builder())
                .scheme("moz-notification")
                .authority(packageName)
                .path(className)
                .appendQueryParameter("name", name)
                .appendQueryParameter("cookie", cookie)
                .build();

        final Intent clickIntent = new Intent(CLICK_ACTION);
        clickIntent.setClass(mContext, NotificationReceiver.class);
        clickIntent.setData(dataUri);

        if (persistentData != null) {
            final Intent persistentIntent = GeckoService.getIntentToCreateServices(
                    mContext, "persistent-notification-click", persistentData);
            clickIntent.putExtra(PERSISTENT_INTENT_EXTRA, persistentIntent);
        }

        final PendingIntent clickPendingIntent = PendingIntent.getBroadcast(
                mContext, 0, clickIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        final Intent closeIntent = new Intent(CLOSE_ACTION);
        closeIntent.setClass(mContext, NotificationReceiver.class);
        closeIntent.setData(dataUri);

        if (persistentData != null) {
            final Intent persistentIntent = GeckoService.getIntentToCreateServices(
                    mContext, "persistent-notification-close", persistentData);
            closeIntent.putExtra(PERSISTENT_INTENT_EXTRA, persistentIntent);
        }

        final PendingIntent closePendingIntent = PendingIntent.getBroadcast(
                mContext, 0, closeIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        add(name, imageUrl, host, title, text, clickPendingIntent, closePendingIntent);
        GeckoAppShell.onNotificationShow(name, cookie);
    }

    @Override // NotificationListener
    public void closeNotification(String name)
    {
        remove(name);
    }

    /**
     * Adds a notification; used for web notifications.
     *
     * @param name           the unique name of the notification
     * @param imageUrl       URL of the image to use
     * @param alertTitle     title of the notification
     * @param alertText      text of the notification
     * @param contentIntent  Intent used when the notification is clicked
     * @param deleteIntent   Intent used when the notification is closed
     */
    private void add(final String name, final String imageUrl, final String host,
                     final String alertTitle, final String alertText,
                     final PendingIntent contentIntent, final PendingIntent deleteIntent) {
        final NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext)
                .setContentTitle(alertTitle)
                .setContentText(alertText)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentIntent(contentIntent)
                .setDeleteIntent(deleteIntent)
                .setAutoCancel(true)
                .setDefaults(Notification.DEFAULT_SOUND)
                .setStyle(new NotificationCompat.BigTextStyle()
                        .bigText(alertText)
                        .setSummaryText(host));

        // Fetch icon.
        if (!imageUrl.isEmpty()) {
            final Bitmap image = BitmapUtils.decodeUrl(imageUrl);
            builder.setLargeIcon(image);
        }

        builder.setWhen(System.currentTimeMillis());
        final Notification notification = builder.build();

        synchronized (this) {
            mNotifications.put(name, notification);
        }

        mNotificationManager.notify(name, 0, notification);
    }

    /**
     * Adds a notification; used for Fennec app notifications.
     *
     * @param name           the unique name of the notification
     * @param notification   the Notification to add
     */
    public synchronized void add(final String name, final Notification notification) {
        final boolean ongoing = isOngoing(notification);

        if (ongoing != isOngoing(mNotifications.get(name))) {
            // In order to change notification from ongoing to non-ongoing, or vice versa,
            // we have to remove the previous notification, because ongoing notifications
            // use a different id value than non-ongoing notifications.
            onNotificationClose(name);
        }

        mNotifications.put(name, notification);

        if (!ongoing) {
            mNotificationManager.notify(name, 0, notification);
            return;
        }

        // Ongoing
        if (mForegroundNotification == null) {
            setForegroundNotificationLocked(name, notification);
        } else if (mForegroundNotification.equals(name)) {
            // Shortcut to update the current foreground notification, instead of
            // going through the service.
            mNotificationManager.notify(R.id.foregroundNotification, notification);
        }
    }

    /**
     * Updates a notification.
     *
     * @param name          Name of existing notification
     * @param progress      progress of item being updated
     * @param progressMax   max progress of item being updated
     * @param alertText     text of the notification
     */
    public void update(final String name, final long progress,
                       final long progressMax, final String alertText) {
        Notification notification;
        synchronized (this) {
            notification = mNotifications.get(name);
        }
        if (notification == null) {
            return;
        }

        notification = new NotificationCompat.Builder(mContext)
                .setContentText(alertText)
                .setSmallIcon(notification.icon)
                .setWhen(notification.when)
                .setContentIntent(notification.contentIntent)
                .setProgress((int) progressMax, (int) progress, false)
                .build();

        add(name, notification);
    }

    /* package */ synchronized Notification onNotificationClose(final String name) {
        mNotificationManager.cancel(name, 0);

        final Notification notification = mNotifications.remove(name);
        if (notification != null) {
            updateForegroundNotificationLocked(name);
        }
        return notification;
    }

    /**
     * Removes a notification.
     *
     * @param name Name of existing notification
     */
    public synchronized void remove(final String name) {
        final Notification notification = onNotificationClose(name);
        if (notification == null || notification.deleteIntent == null) {
            return;
        }

        // Canceling the notification doesn't trigger the delete intent, so we
        // have to trigger it manually.
        try {
            notification.deleteIntent.send();
        } catch (final PendingIntent.CanceledException e) {
            // Ignore.
        }
    }

    /**
     * Determines whether the service is done.
     *
     * The service is considered finished when all notifications have been
     * removed.
     *
     * @return whether all notifications have been removed
     */
    public synchronized boolean isDone() {
        return mNotifications.isEmpty();
    }

    /**
     * Determines whether a notification should hold a foreground service to keep Gecko alive
     *
     * @param name           the name of the notification to check
     * @return               whether the notification is ongoing
     */
    public synchronized boolean isOngoing(final String name) {
        return isOngoing(mNotifications.get(name));
    }

    /**
     * Determines whether a notification should hold a foreground service to keep Gecko alive
     *
     * @param notification   the notification to check
     * @return               whether the notification is ongoing
     */
    public boolean isOngoing(final Notification notification) {
        if (notification != null && (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0) {
            return true;
        }
        return false;
    }

    private void setForegroundNotificationLocked(final String name,
                                                 final Notification notification) {
        mForegroundNotification = name;

        final Intent intent = new Intent(mContext, NotificationService.class);
        intent.putExtra(NotificationService.EXTRA_NOTIFICATION, notification);
        mContext.startService(intent);
    }

    private void updateForegroundNotificationLocked(final String oldName) {
        if (mForegroundNotification == null || !mForegroundNotification.equals(oldName)) {
            return;
        }

        // If we're removing the notification associated with the
        // foreground, we need to pick another active notification to act
        // as the foreground notification.
        for (final String name : mNotifications.keySet()) {
            final Notification notification = mNotifications.get(name);
            if (isOngoing(notification)) {
                setForegroundNotificationLocked(name, notification);
                return;
            }
        }

        setForegroundNotificationLocked(null, null);
    }
}
