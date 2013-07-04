/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;

import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;

import java.util.concurrent.ConcurrentHashMap;

public class NotificationHandler {
    private final ConcurrentHashMap<Integer, AlertNotification>
            mAlertNotifications = new ConcurrentHashMap<Integer, AlertNotification>();
    private final Context mContext;

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
    private AlertNotification mForegroundNotification;

    public NotificationHandler(Context context) {
        mContext = context;
    }

    /**
     * Adds a notification.
     *
     * @param notificationID the unique ID of the notification
     * @param aImageUrl      URL of the image to use
     * @param aAlertTitle    title of the notification
     * @param aAlertText     text of the notification
     * @param contentIntent  Intent used when the notification is clicked
     * @param clearIntent    Intent used when the notification is removed
     */
    public void add(int notificationID, String aImageUrl, String aAlertTitle,
                    String aAlertText, PendingIntent contentIntent) {
        // Remove the old notification with the same ID, if any
        remove(notificationID);

        Uri imageUri = Uri.parse(aImageUrl);
        int icon = BitmapUtils.getResource(imageUri, R.drawable.ic_status_logo);
        final AlertNotification notification = new AlertNotification(mContext, notificationID,
                icon, aAlertTitle, aAlertText, System.currentTimeMillis(), imageUri);

        notification.setLatestEventInfo(mContext, aAlertTitle, aAlertText, contentIntent);

        notification.show();
        mAlertNotifications.put(notification.getId(), notification);
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
        final AlertNotification notification = mAlertNotifications.get(notificationID);
        if (notification == null) {
            return;
        }

        notification.updateProgress(aAlertText, aProgress, aProgressMax);

        if (mForegroundNotification == null && notification.isProgressStyle()) {
            setForegroundNotification(notification);
        }

        // Hide the notification at 100%
        if (aProgress == aProgressMax) {
            remove(notificationID);
        }
    }

    /**
     * Removes a notification.
     *
     * @param notificationID ID of existing notification
     */
    public void remove(int notificationID) {
        final AlertNotification notification = mAlertNotifications.remove(notificationID);
        if (notification != null) {
            updateForegroundNotification(notification);
            notification.cancel();
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
    public boolean isDone() {
        return mAlertNotifications.isEmpty();
    }

    /**
     * Determines whether a notification is showing progress.
     *
     * @param notificationID the notification to check
     * @return               whether the notification is progress style
     */
    public boolean isProgressStyle(int notificationID) {
        final AlertNotification notification = mAlertNotifications.get(notificationID);
        return notification != null && notification.isProgressStyle();
    }

    protected void setForegroundNotification(AlertNotification notification) {
        mForegroundNotification = notification;
    }

    private void updateForegroundNotification(AlertNotification oldNotification) {
        if (mForegroundNotification == oldNotification) {
            // If we're removing the notification associated with the
            // foreground, we need to pick another active notification to act
            // as the foreground notification.
            AlertNotification foregroundNotification = null;
            for (final AlertNotification notification : mAlertNotifications.values()) {
                if (notification.isProgressStyle()) {
                    foregroundNotification = notification;
                    break;
                }
            }

            setForegroundNotification(foregroundNotification);
        }
    }
}
