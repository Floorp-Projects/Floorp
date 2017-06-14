/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.notification;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.web.BrowsingSession;
import org.mozilla.focus.web.WebViewProvider;

/**
 * This service tracks the background/foreground state of our main activity and whether a browsing
 * session is active or not.
 *
 * As long as a browsing session is active we will show a notification. Clicking the notification
 * will erase the session.
 */
public class BrowsingNotificationService extends Service {
    private static final int NOTIFICATION_ID = 83;

    private static final String ACTION_START = "start";
    private static final String ACTION_STOP = "stop";
    private static final String ACTION_ERASE = "erase";
    private static final String ACTION_FOREGROUND = "foreground";
    private static final String ACTION_BACKGROUND = "background";

    private boolean foreground;

    public static void start(Context context) {
        final Intent intent = new Intent(context, BrowsingNotificationService.class);
        intent.setAction(ACTION_START);

        context.startService(intent);
    }

    public static void foreground(Context context) {
        final Intent intent = new Intent(context, BrowsingNotificationService.class);
        intent.setAction(ACTION_FOREGROUND);

        context.startService(intent);
    }

    public static void background(Context context) {
        final Intent intent = new Intent(context, BrowsingNotificationService.class);
        intent.setAction(ACTION_BACKGROUND);

        context.startService(intent);
    }

    public static void stop(Context context) {
        final Intent intent = new Intent(context, BrowsingNotificationService.class);
        intent.setAction(ACTION_STOP);

        context.startService(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        switch (intent.getAction()) {
            case ACTION_START:
                foreground = true;
                startBrowsingSession();
                break;

            case ACTION_STOP:
                stopBrowsingSession();
                break;

            case ACTION_FOREGROUND:
                foreground = true;
                break;

            case ACTION_BACKGROUND:
                foreground = false;
                break;

            case ACTION_ERASE:
                final Intent activityIntent = new Intent(this, MainActivity.class);
                activityIntent.setAction(MainActivity.ACTION_ERASE);
                activityIntent.putExtra(MainActivity.EXTRA_FINISH, !foreground);

                if (!foreground) {
                    activityIntent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
                }

                // This doesn't seem to be needed on Android 7.1. It is needed on Android 6, or otherwise
                // we crash with "AndroidRuntimeException: Calling startActivity() from outside of an Activity context requires the FLAG_ACTIVITY_NEW_TASK flag."
                activityIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                startActivity(activityIntent);

                TelemetryWrapper.eraseNotificationEvent();
                break;

            default:
                throw new IllegalStateException("Unknown intent: " + intent);
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        // If our task got removed then we might have been killed in the task switcher. In this case
        // our activity had no chance to cleanup the browsing data. Let's try to do it from here.
        WebViewProvider.performCleanup(this);

        stopBrowsingSession();
    }

    private void startBrowsingSession() {
        BrowsingSession.getInstance().start();

        startForeground(NOTIFICATION_ID, buildNotification());
    }

    private void stopBrowsingSession() {
        BrowsingSession.getInstance().stop();

        stopForeground(true);
        stopSelf();
    }

    private Notification buildNotification() {
        final Intent intent = new Intent(this, BrowsingNotificationService.class);
        intent.setAction(ACTION_ERASE);

        final PendingIntent pendingIntent = PendingIntent.getService(this, 0, intent, PendingIntent.FLAG_ONE_SHOT);

        return new NotificationCompat.Builder(this)
                .setOngoing(true)
                .setSmallIcon(R.drawable.ic_notification)
                .setContentTitle(getString(R.string.app_name))
                .setContentText(getString(R.string.notification_erase_text))
                .setContentIntent(pendingIntent)
                .setVisibility(Notification.VISIBILITY_SECRET)
                .setShowWhen(false)
                .setLocalOnly(true)
                .build();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
