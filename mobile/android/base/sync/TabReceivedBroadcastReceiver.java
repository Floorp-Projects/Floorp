/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.tabqueue.TabQueueDispatcher;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

/**
 * A BroadcastReceiver that displays a notification for a tab sent to this device.
 *
 * The expected Intent should contain:
 *   * Data: URI to open in the notification
 *   * EXTRA_TITLE: Page title of the URI to open
 */
public class TabReceivedBroadcastReceiver extends BroadcastReceiver {
    private static final String LOGTAG = TabReceivedBroadcastReceiver.class.getSimpleName();

    private static final String PREF_NOTIFICATION_ID = "tab_received_notification_id";

    public static final String EXTRA_TITLE = "org.mozilla.gecko.extra.TITLE";

    private static final int MAX_NOTIFICATION_COUNT = 1000;

    @Override
    public void onReceive(final Context context, final Intent intent) {
        // BroadcastReceivers don't keep the process alive so
        // we need to do this every time. Ideally, we wouldn't.
        final Resources res = context.getResources();
        BrowserLocaleManager.getInstance().correctLocale(context, res, res.getConfiguration());

        final String uri = intent.getDataString();
        if (uri == null) {
            Log.d(LOGTAG, "Received null uri – ignoring");
            return;
        }

        final String title = intent.getStringExtra(EXTRA_TITLE);
        String notificationTitle = context.getString(R.string.sync_new_tab);
        if (title != null) {
            notificationTitle = notificationTitle.concat(": " + title);
        }

        final Intent notificationIntent = new Intent(Intent.ACTION_VIEW, intent.getData());
        notificationIntent.putExtra(TabQueueDispatcher.SKIP_TAB_QUEUE_FLAG, true);
        final PendingIntent contentIntent = PendingIntent.getActivity(context, 0, notificationIntent, 0);

        final NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
        builder.setSmallIcon(R.drawable.flat_icon);
        builder.setContentTitle(notificationTitle);
        builder.setWhen(System.currentTimeMillis());
        builder.setAutoCancel(true);
        builder.setContentText(uri);
        builder.setContentIntent(contentIntent);

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        final int notificationId = getNextNotificationId(prefs.getInt(PREF_NOTIFICATION_ID, 0));
        final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.notify(notificationId, builder.build());

        // BroadcastReceivers do not continue async methods
        // once onReceive returns so we must commit synchronously.
        prefs.edit().putInt(PREF_NOTIFICATION_ID, notificationId).commit();
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

