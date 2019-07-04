package org.mozilla.gecko;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.gecko.notifications.NotificationHelper;

import java.io.File;
import java.io.IOException;

public class CrashHandlerService extends Service {
    private static final String LOGTAG = "CrashHandlerService";
    private static final String ACTION_STOP = "action_stop";

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (intent == null) {
            // no-op
            // The Intent may be null if the service is being restarted after its process has gone away,
            // and it had previously returned anything except START_STICKY_COMPATIBILITY.
        } else if (ACTION_STOP.equals(intent.getAction())) {
            dismissNotification();
        } else {
            // Notify GeckoApp that we've crashed, so it can react appropriately during the next start.
            try {
                File crashFlag = new File(GeckoProfileDirectories.getMozillaDirectory(this), "CRASHED");
                crashFlag.createNewFile();
            } catch (GeckoProfileDirectories.NoMozillaDirectoryException | IOException e) {
                Log.e(LOGTAG, "Cannot set crash flag: ", e);
            }

            intent.setClass(this, CrashReporterActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            if (AppConstants.Versions.feature26Plus) {
                startCrashHandling(intent);
            } else {
                startActivity(intent);
                stopSelf();
            }
        }

        return Service.START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    /**
     * Call this for any necessary cleanup like removing the foreground notification shown on Android Q+.
     */
    public static void reportingStarted(final Context context) {
        if (AppConstants.Versions.feature26Plus) {
            final Intent intent = new Intent(context, CrashHandlerService.class);
            intent.setAction(ACTION_STOP);
            context.startService(intent);
        }
    }

    @TargetApi(Build.VERSION_CODES.O)
    private Notification getActivityNotification(final Context context, final Intent activityIntent) {
        final Intent dismissNotificationIntent = new Intent(ACTION_STOP, null, this, this.getClass());
        final PendingIntent dismissNotification = PendingIntent.getService(this, 0, dismissNotificationIntent, PendingIntent.FLAG_CANCEL_CURRENT);
        final PendingIntent startReporterActivity = PendingIntent.getActivity(this, 0, activityIntent, 0);
        final String notificationChannelId = NotificationHelper.getInstance(context)
                .getNotificationChannel(NotificationHelper.Channel.CRASH_HANDLER).getId();

        return new Notification.Builder(this, notificationChannelId)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentTitle(getString(R.string.crash_notification_title))
                .setContentText(getString(R.string.crash_notification_message))
                .setDefaults(Notification.DEFAULT_ALL)
                .setContentIntent(startReporterActivity)
                .addAction(0, getString(R.string.crash_notification_negative_button_text), dismissNotification)
                .build();
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void dismissNotification() {
        stopForeground(Service.STOP_FOREGROUND_REMOVE);
        stopSelf();
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void startCrashHandling(final Intent activityIntent) {
        // Piggy-back the SYSTEM_ALERT_WINDOW permission given by the user for the Tab Queue functionality.
        // Otherwise fallback to display a foreground notification, this being the only way we can
        // start an activity from background.
        // https://developer.android.com/preview/privacy/background-activity-starts#conditions-allow-activity-starts
        if (Settings.canDrawOverlays(this)) {
            startActivity(activityIntent);
        } else {
            final Notification notification = getActivityNotification(this, activityIntent);

            // Android O+ will show a non-intrusive notification (needed for a foreground service)
            // and then show the crash reporter immediately
            // Android Q+ will show an intrusive notification which when acted upon by the user will
            // allow starting the crash reporter. See http://g.co/dev/bgblock
            startForeground(R.id.mediaControlNotification, notification);
            if (AppConstants.Versions.feature26Plus && AppConstants.Versions.preQ) {
                startActivity(activityIntent);
            }
        }
    }
}
