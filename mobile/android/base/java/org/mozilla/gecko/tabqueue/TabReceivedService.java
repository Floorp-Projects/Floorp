/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.database.Cursor;
import android.media.RingtoneManager;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.support.v4.app.JobIntentService;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;

/**
 * A JobIntentService that displays a notification for a tab sent to this device.
 *
 * The expected Intent should contain:
 *   * Data: URI to open in the notification
 *   * EXTRA_TITLE: Page title of the URI to open
 */
public class TabReceivedService extends JobIntentService {
    private static final String LOGTAG = "Gecko" + TabReceivedService.class.getSimpleName();

    private static final String PREF_NOTIFICATION_ID = "tab_received_notification_id";

    private static final int MAX_NOTIFICATION_COUNT = 1000;

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        // JobIntentServices doesn't keep the process alive so
        // we need to do this every time. Ideally, we wouldn't.
        final Resources res = getResources();
        BrowserLocaleManager.getInstance().correctLocale(this, res, res.getConfiguration());

        final String uri = intent.getDataString();
        if (uri == null) {
            Log.d(LOGTAG, "Received null uri – ignoring");
            return;
        }

        final Intent notificationIntent = new Intent(Intent.ACTION_VIEW, intent.getData());
        notificationIntent.putExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, true);
        final PendingIntent contentIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);

        final String notificationTitle = getNotificationTitle(intent.getStringExtra(BrowserContract.EXTRA_CLIENT_GUID));
        final NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
        builder.setSmallIcon(R.drawable.ic_status_logo);
        builder.setContentTitle(notificationTitle);
        builder.setWhen(System.currentTimeMillis());
        builder.setAutoCancel(true);
        builder.setContentText(uri);
        builder.setContentIntent(contentIntent);

        // Trigger "heads-up" notification mode on supported Android versions.
        builder.setPriority(NotificationCompat.PRIORITY_HIGH);
        final Uri notificationSoundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        if (notificationSoundUri != null) {
            builder.setSound(notificationSoundUri);
        }

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
        final int notificationId = getNextNotificationId(prefs.getInt(PREF_NOTIFICATION_ID, 0));
        final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(this);
        notificationManager.notify(notificationId, builder.build());

        // Save the ID last so if the Service is killed and the Intent is redelivered,
        // the ID is unlikely to have been updated and we would re-use the the old one.
        // This would prevent two identical notifications from appearing if the
        // notification was shown during the previous Intent processing attempt.
        prefs.edit().putInt(PREF_NOTIFICATION_ID, notificationId).apply();
    }

    @Override
    public boolean onStopCurrentWork() {
        // Reschedule the work if it has been stopped before completing (shouldn't)
        return true;
    }

    /**
     * @param clientGUID the guid of the client in the clients table
     * @return the client's name from the clients table, if possible, else the brand name.
     */
    @WorkerThread
    private String getNotificationTitle(@Nullable final String clientGUID) {
        if (clientGUID == null) {
            Log.w(LOGTAG, "Received null guid, using brand name.");
            return AppConstants.MOZ_APP_DISPLAYNAME;
        }

        final Cursor c = getContentResolver().query(BrowserContract.Clients.CONTENT_URI,
                new String[] { BrowserContract.Clients.NAME },
                BrowserContract.Clients.GUID + "=?", new String[] { clientGUID }, null);
        try {
            if (c != null && c.moveToFirst()) {
                return c.getString(c.getColumnIndex(BrowserContract.Clients.NAME));
            } else {
                Log.w(LOGTAG, "Device not found, using brand name.");
                return AppConstants.MOZ_APP_DISPLAYNAME;
            }
        } finally {
            if (c != null) {
                c.close();
            }
        }
    }

    /**
     * Notification IDs must be unique else a notification
     * will be overwritten so we cycle them.
     */
    private int getNextNotificationId(final int currentId) {
        if (currentId > MAX_NOTIFICATION_COUNT) {
            return 0;
        } else {
            return currentId + 1;
        }
    }
}
