/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;

public class NotificationService extends Service {
    private final IBinder mBinder = new NotificationBinder();
    private NotificationHandler mHandler;

    @Override
    public void onCreate() {
        // This has to be initialized in onCreate in order to ensure that the NotificationHandler can
        // access the NotificationManager service.
        mHandler = new NotificationHandler(this) {
            @Override
            protected void setForegroundNotification(int id, Notification notification) {
                super.setForegroundNotification(id, notification);

                if (notification == null) {
                    stopForeground(true);
                } else {
                    startForeground(id, notification);
                }
            }
        };
    }

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

    public NotificationHandler getNotificationHandler() {
        return mHandler;
    }
}
