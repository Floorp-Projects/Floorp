/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.tabqueue.TabQueueDispatcher;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.util.Log;

/**
 * An IntentService that displays a notification for a tab sent to this device.
 *
 * The expected Intent should contain:
 *   * Data: URI to open in the notification
 *   * EXTRA_TITLE: Page title of the URI to open
 */
public class TabReceivedService extends IntentService {
    private static final String LOGTAG = "Gecko" + TabReceivedService.class.getSimpleName();

    private static final String PREF_NOTIFICATION_ID = "tab_received_notification_id";

    public static final String EXTRA_TITLE = "org.mozilla.gecko.extra.TITLE";

    private static final int MAX_NOTIFICATION_COUNT = 1000;

    public TabReceivedService() {
        super(LOGTAG);
    }

    @Override
    protected void onHandleIntent(final Intent intent) {
        // IntentServices don't keep the process alive so
        // we need to do this every time. Ideally, we wouldn't.
        final Resources res = getResources();
        BrowserLocaleManager.getInstance().correctLocale(this, res, res.getConfiguration());

        final String uri = intent.getDataString();
        if (uri == null) {
            Log.d(LOGTAG, "Received null uri – ignoring");
            return;
        }

        final String title = intent.getStringExtra(EXTRA_TITLE);
        String notificationTitle = this.getString(R.string.sync_new_tab);
        if (title != null) {
            notificationTitle = notificationTitle.concat(": " + title);
        }

        final Intent notificationIntent = new Intent(Intent.ACTION_VIEW, intent.getData());
        notificationIntent.putExtra(TabQueueDispatcher.SKIP_TAB_QUEUE_FLAG, true);
        final PendingIntent contentIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);

        final NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
        builder.setSmallIcon(R.drawable.flat_icon);
        builder.setContentTitle(notificationTitle);
        builder.setWhen(System.currentTimeMillis());
        builder.setAutoCancel(true);
        builder.setContentText(uri);
        builder.setContentIntent(contentIntent);

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(this);
        final int notificationId = getNextNotificationId(prefs.getInt(PREF_NOTIFICATION_ID, 0));
        final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(this);
        notificationManager.notify(notificationId, builder.build());

        prefs.edit().putInt(PREF_NOTIFICATION_ID, notificationId).apply();
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

