/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.action;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.feeds.FeedAlarmReceiver;
import org.mozilla.gecko.feeds.FeedService;

/**
 * SetupAlarmsAction: Set up alarms to run various actions every now and then.
 */
public class SetupAlarmsAction extends FeedAction {
    private static final String LOGTAG = "FeedSetupAction";

    private Context context;

    public SetupAlarmsAction(Context context) {
        this.context = context;
    }

    @Override
    public void perform(BrowserDB browserDB, Intent intent) {
        final AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);

        cancelPreviousAlarms(alarmManager);
        scheduleAlarms(alarmManager);
    }

    @Override
    public boolean requiresNetwork() {
        return false;
    }

    @Override
    public boolean requiresPreferenceEnabled() {
        return false;
    }

    private void cancelPreviousAlarms(AlarmManager alarmManager) {
        final PendingIntent withdrawIntent = getWithdrawPendingIntent();
        alarmManager.cancel(withdrawIntent);

        final PendingIntent enrollIntent = getEnrollPendingIntent();
        alarmManager.cancel(enrollIntent);

        final PendingIntent checkIntent = getCheckPendingIntent();
        alarmManager.cancel(checkIntent);

        log("Cancelled previous alarms");
    }

    private void scheduleAlarms(AlarmManager alarmManager) {
        alarmManager.setInexactRepeating(
                AlarmManager.ELAPSED_REALTIME,
                SystemClock.elapsedRealtime() + AlarmManager.INTERVAL_FIFTEEN_MINUTES,
                AlarmManager.INTERVAL_DAY,
                getWithdrawPendingIntent());

        alarmManager.setInexactRepeating(
                AlarmManager.ELAPSED_REALTIME,
                SystemClock.elapsedRealtime() + AlarmManager.INTERVAL_HALF_HOUR,
                AlarmManager.INTERVAL_DAY,
                getEnrollPendingIntent()
        );

        alarmManager.setInexactRepeating(
                AlarmManager.ELAPSED_REALTIME,
                SystemClock.elapsedRealtime() + AlarmManager.INTERVAL_HOUR,
                AlarmManager.INTERVAL_HALF_DAY,
                getCheckPendingIntent()
        );

        log("Scheduled alarms");
    }

    private PendingIntent getWithdrawPendingIntent() {
        Intent intent = new Intent(context, FeedAlarmReceiver.class);
        intent.setAction(FeedService.ACTION_WITHDRAW);
        return PendingIntent.getBroadcast(context, 0, intent, 0);
    }

    private PendingIntent getEnrollPendingIntent() {
        Intent intent = new Intent(context, FeedAlarmReceiver.class);
        intent.setAction(FeedService.ACTION_ENROLL);
        return PendingIntent.getBroadcast(context, 0, intent, 0);
    }

    private PendingIntent getCheckPendingIntent() {
        Intent intent = new Intent(context, FeedAlarmReceiver.class);
        intent.setAction(FeedService.ACTION_CHECK);
        return PendingIntent.getBroadcast(context, 0, intent, 0);
    }
}
