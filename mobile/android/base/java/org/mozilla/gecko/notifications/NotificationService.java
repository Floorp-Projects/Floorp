/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

public final class NotificationService extends Service {
    /**
     * Start Intent of the service must contain a notification extra.
     */
    public static final String EXTRA_NOTIFICATION = "notification";
    /**
     * Stop Intent of the service must contain the stop action extra.
     */
    public static final String EXTRA_ACTION_STOP = "action_stop";

    /**
     * We are holding a reference to the notification this service was started with in order to use it
     * again when we queue the stop action thus satisfying the Oreo limitations.
     */
    private Notification currentNotification;

    @Override // Service
    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        handleIntent(intent, startId);
        return START_NOT_STICKY;
    }

    @Override // Service
    public IBinder onBind(final Intent intent) {
        return null;
    }

    private void handleIntent(Intent intent, int startId) {
        //Start Intent
        if (intent.hasExtra(EXTRA_NOTIFICATION)) {
            currentNotification = intent.getParcelableExtra(EXTRA_NOTIFICATION);
            startForeground(R.id.foregroundNotification, currentNotification);
        }

        //Stop Intent
        if (intent.hasExtra(NotificationService.EXTRA_ACTION_STOP)) {
            //Call necessary for Oreo+ only
            if (!AppConstants.Versions.preO) {
                startForeground(R.id.foregroundNotification, currentNotification);
            }

            stopForeground(true);
            stopSelfResult(startId);
        }
    }
}
