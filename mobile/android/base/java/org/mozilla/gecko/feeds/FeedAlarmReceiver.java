/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds;

import android.content.Context;
import android.content.Intent;
import android.support.v4.content.WakefulBroadcastReceiver;
import android.util.Log;

/**
 * Broadcast receiver that will receive broadcasts from the AlarmManager and start the FeedService
 * with the given action.
 */
public class FeedAlarmReceiver extends WakefulBroadcastReceiver {
    private static final String LOGTAG = "FeedCheckAction";

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();

        Log.d(LOGTAG, "Received alarm with action: " + action);

        final Intent serviceIntent = new Intent(context, FeedService.class);
        serviceIntent.setAction(action);

        startWakefulService(context, serviceIntent);
    }
}
