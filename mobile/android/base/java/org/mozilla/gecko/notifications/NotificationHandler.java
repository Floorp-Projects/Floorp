/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.graphics.Bitmap;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;

import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;

import java.util.concurrent.ConcurrentHashMap;

public class NotificationHandler {
    private static String LOGTAG = "GeckoNotifHandler";
    private final ConcurrentHashMap<String, Notification>
            mNotifications = new ConcurrentHashMap<>();
    private final Context mContext;
    private final NotificationManagerCompat mNotificationManager;

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
    private Notification mForegroundNotification;
    private String mForegroundNotificationName;

    public NotificationHandler(Context context) {
        mContext = context;
        mNotificationManager = NotificationManagerCompat.from(context);
    }

    /**
     * Adds a notification.
     *
     * @param aName          the unique name of the notification
     * @param aImageUrl      URL of the image to use
     * @param aAlertTitle    title of the notification
     * @param aAlertText     text of the notification
     * @param contentIntent  Intent used when the notification is clicked
     * @param deleteIntent   Intent used when the notification is closed
     */
    public void add(final String aName, String aImageUrl, String aHost, String aAlertTitle,
                    String aAlertText, PendingIntent contentIntent, PendingIntent deleteIntent) {
        // Remove the old notification with the same ID, if any
        remove(aName);

        final NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext)
                .setContentTitle(aAlertTitle)
                .setContentText(aAlertText)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentIntent(contentIntent)
                .setDeleteIntent(deleteIntent)
                .setAutoCancel(true)
                .setStyle(new NotificationCompat.InboxStyle()
                          .addLine(aAlertText)
                          .setSummaryText(aHost));

        // Fetch icon.
        if (!aImageUrl.isEmpty()) {
            final Bitmap image = BitmapUtils.decodeUrl(aImageUrl);
            builder.setLargeIcon(image);
        }

        builder.setWhen(System.currentTimeMillis());
        final Notification notification = builder.build();

        mNotificationManager.notify(aName, 0, notification);
        mNotifications.put(aName, notification);
    }

    /**
     * Adds a notification.
     *
     * @param name           the unique name of the notification
     * @param notification   the Notification to add
     */
    public void add(String name, Notification notification) {
        if (isOngoing(notification)) {
            // Cancel any previous non-ongoing notifications with the same name.
            mNotificationManager.cancel(name, 0);

            if (mForegroundNotificationName == null) {
                setForegroundNotification(name, notification);
            } else if (name.equals(mForegroundNotificationName)) {
                mNotificationManager.notify(R.id.foregroundNotification, notification);
            }

        } else {
            // Cancel any previous ongoing notifications with the same name.
            updateForegroundNotification(name);
            mNotificationManager.notify(name, 0, notification);
        }

        mNotifications.put(name, notification);
    }

    /**
     * Updates a notification.
     *
     * @param aName Name of existing notification
     * @param aProgress      progress of item being updated
     * @param aProgressMax   max progress of item being updated
     * @param aAlertText     text of the notification
     */
    public void update(String aName, long aProgress, long aProgressMax, String aAlertText) {
        Notification notification = mNotifications.get(aName);
        if (notification == null) {
            return;
        }

        notification = new NotificationCompat.Builder(mContext)
                .setContentText(aAlertText)
                .setSmallIcon(notification.icon)
                .setWhen(notification.when)
                .setContentIntent(notification.contentIntent)
                .setProgress((int) aProgressMax, (int) aProgress, false)
                .build();

        add(aName, notification);
    }

    /**
     * Removes a notification.
     *
     * @param aName Name of existing notification
     */
    public void remove(String aName) {
        final Notification notification = mNotifications.remove(aName);
        if (notification != null) {
            updateForegroundNotification(aName);
        }
        mNotificationManager.cancel(aName, 0);
    }

    /**
     * Determines whether the service is done.
     *
     * The service is considered finished when all notifications have been
     * removed.
     *
     * @return whether all notifications have been removed
     */
    public boolean isDone() {
        return mNotifications.isEmpty();
    }

    /**
     * Determines whether a notification should hold a foreground service to keep Gecko alive
     *
     * @param aName          the name of the notification to check
     * @return               whether the notification is ongoing
     */
    public boolean isOngoing(String aName) {
        final Notification notification = mNotifications.get(aName);
        return isOngoing(notification);
    }

    /**
     * Determines whether a notification should hold a foreground service to keep Gecko alive
     *
     * @param notification   the notification to check
     * @return               whether the notification is ongoing
     */
    public boolean isOngoing(Notification notification) {
        if (notification != null && (notification.flags & Notification.FLAG_ONGOING_EVENT) > 0) {
            return true;
        }
        return false;
    }

    protected void setForegroundNotification(String name, Notification notification) {
        mForegroundNotificationName = name;
        mForegroundNotification = notification;
    }

    private void updateForegroundNotification(String oldName) {
        if (oldName != null && oldName.equals(mForegroundNotificationName)) {
            // If we're removing the notification associated with the
            // foreground, we need to pick another active notification to act
            // as the foreground notification.
            Notification foregroundNotification = null;
            String foregroundName = null;
            for (final String name : mNotifications.keySet()) {
                final Notification notification = mNotifications.get(name);
                if (isOngoing(notification)) {
                    foregroundNotification = notification;
                    foregroundName = name;
                    break;
                }
            }
            setForegroundNotification(foregroundName, foregroundNotification);
        }
    }
}
