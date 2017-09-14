/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.telemetry.TelemetryWrapper;

/**
 * As long as a session is active this service will keep the notification (and our process) alive.
 */
public class SessionNotificationService extends Service {
    private static final int NOTIFICATION_ID = 83;

    private static final String NOTIFICATION_CHANNEL_ID = "browsing-session";

    private static final String ACTION_START = "start";
    private static final String ACTION_ERASE = "erase";

    /* package */ static void start(Context context) {
        final Intent intent = new Intent(context, SessionNotificationService.class);
        intent.setAction(ACTION_START);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.startForegroundService(intent);
        } else {
            context.startService(intent);
        }
    }

    /* package */ static void stop(Context context) {
        final Intent intent = new Intent(context, SessionNotificationService.class);

        context.stopService(intent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        final String action = intent.getAction();
        if (action == null) {
            return START_NOT_STICKY;
        }

        switch (action) {
            case ACTION_START:
                createNotificationChannelIfNeeded();
                startForeground(NOTIFICATION_ID, buildNotification());
                break;

            case ACTION_ERASE:
                TelemetryWrapper.eraseNotificationEvent();

                SessionManager.getInstance().removeAllSessions();

                VisibilityLifeCycleCallback.finishAndRemoveTaskIfInBackground(this);
                break;

            default:
                throw new IllegalStateException("Unknown intent: " + intent);
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        TelemetryWrapper.eraseTaskRemoved();

        SessionManager.getInstance().removeAllSessions();

        stopForeground(true);
        stopSelf();
    }

    private Notification buildNotification() {
        return new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setOngoing(true)
                .setSmallIcon(R.drawable.ic_notification)
                .setContentTitle(getString(R.string.app_name))
                .setContentText(getString(R.string.notification_erase_text))
                .setContentIntent(createNotificationIntent())
                .setVisibility(Notification.VISIBILITY_SECRET)
                .setShowWhen(false)
                .setLocalOnly(true)
                .setColor(ContextCompat.getColor(this, R.color.colorErase))
                .addAction(new NotificationCompat.Action(
                        R.drawable.ic_notification,
                        getString(R.string.notification_action_open),
                        createOpenActionIntent()))
                .addAction(new NotificationCompat.Action(
                        R.drawable.ic_delete,
                        getString(R.string.notification_action_erase_and_open),
                        createOpenAndEraseActionIntent()))
                .build();
    }

    private PendingIntent createNotificationIntent() {
        final Intent intent = new Intent(this, SessionNotificationService.class);
        intent.setAction(ACTION_ERASE);

        return PendingIntent.getService(this, 0, intent, PendingIntent.FLAG_ONE_SHOT);
    }

    private PendingIntent createOpenActionIntent() {
        final Intent intent = new Intent(this, MainActivity.class);
        intent.setAction(MainActivity.ACTION_OPEN);

        return PendingIntent.getActivity(this, 1, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntent createOpenAndEraseActionIntent() {
        final Intent intent = new Intent(this, MainActivity.class);

        intent.setAction(MainActivity.ACTION_ERASE);
        intent.putExtra(MainActivity.EXTRA_NOTIFICATION, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return PendingIntent.getActivity(this, 2, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    public void createNotificationChannelIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // Notification channels are only available on Android O or higher.
            return;
        }

        final NotificationManager notificationManager = getSystemService(NotificationManager.class);
        if (notificationManager == null) {
            return;
        }

        final String notificationChannelName = getString(R.string.notification_browsing_session_channel_name);
        final String notificationChannelDescription = getString(
                R.string.notification_browsing_session_channel_description,
                getString(R.string.app_name));

        final NotificationChannel channel = new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, notificationChannelName, NotificationManager.IMPORTANCE_MIN);
        channel.setImportance(NotificationManager.IMPORTANCE_LOW);
        channel.setDescription(notificationChannelDescription);
        channel.enableLights(false);
        channel.enableVibration(false);
        channel.setShowBadge(true);

        notificationManager.createNotificationChannel(channel);
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
