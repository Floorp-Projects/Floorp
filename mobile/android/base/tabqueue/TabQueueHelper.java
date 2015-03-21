/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.support.v4.app.NotificationCompat;
import org.json.JSONArray;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;

public class TabQueueHelper {
    private static final String LOGTAG = "Gecko" + TabQueueHelper.class.getSimpleName();

    public static final String FILE_NAME = "tab_queue_url_list.json";
    public static final String LOAD_URLS_ACTION = "TAB_QUEUE_LOAD_URLS_ACTION";
    public static final int TAB_QUEUE_NOTIFICATION_ID = R.id.tabQueueNotification;

    /**
     * Reads file and converts any content to JSON, adds passed in URL to the data and writes back to the file,
     * creating the file if it doesn't already exist.  This should not be run on the UI thread.
     *
     * @param profile
     * @param url      URL to add
     * @param filename filename to add URL to
     * @return the number of tabs currently queued
     */
    public static int queueURL(final GeckoProfile profile, final String url, final String filename) {
        ThreadUtils.assertNotOnUiThread();

        JSONArray jsonArray = profile.readJSONArrayFromFile(filename);

        jsonArray.put(url);

        profile.writeFile(filename, jsonArray.toString());

        return jsonArray.length();
    }

    /**
     * Displays a notification showing the total number of tabs queue.  If there is already a notification displayed, it
     * will be replaced.
     *
     * @param context
     * @param tabsQueued
     */
    static public void showNotification(Context context, int tabsQueued) {
        Intent resultIntent = new Intent(context, BrowserApp.class);
        resultIntent.setAction(TabQueueHelper.LOAD_URLS_ACTION);

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, resultIntent, PendingIntent.FLAG_CANCEL_CURRENT);

        String title, text;
        final Resources resources = context.getResources();
        if(tabsQueued == 1) {
            title = resources.getString(R.string.tab_queue_notification_title_singular);
            text = resources.getString(R.string.tab_queue_notification_text_singular);
        } else {
            title = resources.getString(R.string.tab_queue_notification_title_plural);
            text = resources.getString(R.string.tab_queue_notification_text_plural, tabsQueued);
        }

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context)
                                                                   .setSmallIcon(R.drawable.ic_status_logo)
                                                                   .setContentTitle(title)
                                                                   .setContentText(text)
                                                                   .setContentIntent(pendingIntent);

        NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(TabQueueHelper.TAB_QUEUE_NOTIFICATION_ID, builder.build());
    }
}