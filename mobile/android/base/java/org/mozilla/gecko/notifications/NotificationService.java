/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.mozilla.gecko.R;

public final class NotificationService extends Service {
    public static final String EXTRA_NOTIFICATION = "notification";

    @Override // Service
    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        final Notification notification = intent.getParcelableExtra(EXTRA_NOTIFICATION);
        if (notification != null) {
            // Start foreground notification.
            startForeground(R.id.foregroundNotification, notification);
            return START_NOT_STICKY;
        }

        // Stop foreground notification
        stopForeground(true);
        stopSelfResult(startId);
        return START_NOT_STICKY;
    }

    @Override // Service
    public IBinder onBind(final Intent intent) {
        return null;
    }
}
