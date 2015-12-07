/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

import java.util.concurrent.ConcurrentHashMap;

public class NotificationHandler {
    private final ConcurrentHashMap<Integer, Notification>
            mNotifications = new ConcurrentHashMap<Integer, Notification>();
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
    private int mForegroundNotificationId;

    public NotificationHandler(Context context) {
        mContext = context;
        mNotificationManager = NotificationManagerCompat.from(context);
    }

    /**
     * Adds a notification.
     *
     * @param notificationID the unique ID of the notification
     * @param aImageUrl      URL of the image to use
     * @param aAlertTitle    title of the notification
     * @param aAlertText     text of the notification
     * @param contentIntent  Intent used when the notification is clicked
     */
    public void add(int notificationID, String aImageUrl, String aAlertTitle,
                    String aAlertText, PendingIntent contentIntent) {
        // Remove the old notification with the same ID, if any
        remove(notificationID);

        Uri imageUri = Uri.parse(aImageUrl);
        int icon = BitmapUtils.getResource(imageUri, R.drawable.ic_status_logo);

        Notification notification = new NotificationCompat.Builder(mContext)
                .setContentTitle(aAlertTitle)
                .setContentText(aAlertText)
                .setSmallIcon(icon)
                .setWhen(System.currentTimeMillis())
                .setContentIntent(contentIntent)
                .build();

        mNotificationManager.notify(notificationID, notification);
        mNotifications.put(notificationID, notification);
    }

    /**
     * Adds a notification.
     *
     * @param id             the unique ID of the notification
     * @param notification   the Notification to add
     */
    public void add(int id, Notification notification) {
        mNotificationManager.notify(id, notification);
        mNotifications.put(id, notification);

        if (mForegroundNotification == null && isOngoing(notification)) {
            setForegroundNotification(id, notification);
        }
    }

    /**
     * Updates a notification.
     *
     * @param notificationID ID of existing notification
     * @param aProgress      progress of item being updated
     * @param aProgressMax   max progress of item being updated
     * @param aAlertText     text of the notification
     */
    public void update(int notificationID, long aProgress, long aProgressMax, String aAlertText) {
        Notification notification = mNotifications.get(notificationID);
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

        add(notificationID, notification);
    }

    /**
     * Removes a notification.
     *
     * @param notificationID ID of existing notification
     */
    public void remove(int notificationID) {
        final Notification notification = mNotifications.remove(notificationID);
        if (notification != null) {
            updateForegroundNotification(notificationID, notification);
        }
        mNotificationManager.cancel(notificationID);
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
     * @param notificationID the id of the notification to check
     * @return               whether the notification is ongoing
     */
    public boolean isOngoing(int notificationID) {
        final Notification notification = mNotifications.get(notificationID);
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

    protected void setForegroundNotification(int id, Notification notification) {
        mForegroundNotificationId = id;
        mForegroundNotification = notification;
    }

    private void updateForegroundNotification(int oldId, Notification oldNotification) {
        if (mForegroundNotificationId == oldId) {
            // If we're removing the notification associated with the
            // foreground, we need to pick another active notification to act
            // as the foreground notification.
            Notification foregroundNotification = null;
            int foregroundId = 0;
            for (final Integer id : mNotifications.keySet()) {
                final Notification notification = mNotifications.get(id);
                if (isOngoing(notification)) {
                    foregroundNotification = notification;
                    foregroundId = id;
                    break;
                }
            }
            setForegroundNotification(foregroundId, foregroundNotification);
        }
    }
}
