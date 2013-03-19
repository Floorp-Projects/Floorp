/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.reflect.Field;
import java.util.concurrent.ConcurrentHashMap;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.Binder;
import android.os.IBinder;

public class NotificationService extends Service {
    private final IBinder mBinder = new NotificationBinder();

    private final ConcurrentHashMap<Integer, AlertNotification>
            mAlertNotifications = new ConcurrentHashMap<Integer, AlertNotification>();

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

    public class NotificationBinder extends Binder {
        NotificationService getService() {
            // Return this instance of NotificationService so clients can call public methods
            return NotificationService.this;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
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

        int icon = R.drawable.ic_status_logo;

        Uri imageUri = Uri.parse(aImageUrl);
        final String scheme = imageUri.getScheme();
        if ("drawable".equals(scheme)) {
            String resource = imageUri.getSchemeSpecificPart();
            resource = resource.substring(resource.lastIndexOf('/') + 1);
            try {
                final Class<R.drawable> drawableClass = R.drawable.class;
                final Field f = drawableClass.getField(resource);
                icon = f.getInt(null);
            } catch (final Exception e) {} // just means the resource doesn't exist
            imageUri = null;
        }

        final AlertNotification notification = new AlertNotification(this, notificationID,
                icon, aAlertTitle, aAlertText, System.currentTimeMillis(), imageUri);

        notification.setLatestEventInfo(this, aAlertTitle, aAlertText, contentIntent);

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

    private void setForegroundNotification(AlertNotification notification) {
        mForegroundNotification = notification;
        if (notification == null) {
            stopForeground(true);
        } else {
            startForeground(notification.getId(), notification);
        }
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
