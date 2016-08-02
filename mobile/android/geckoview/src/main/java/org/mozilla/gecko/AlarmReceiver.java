/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;

import java.util.Timer;
import java.util.TimerTask;

public class AlarmReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        PowerManager powerManager = (PowerManager)context.getSystemService(Context.POWER_SERVICE);
        final WakeLock wakeLock = powerManager.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP, "GeckoAlarm");
        wakeLock.acquire();

        AlarmReceiver.notifyAlarmFired();
        TimerTask releaseLockTask = new TimerTask() {
            @Override
            public void run() {
                wakeLock.release();
            }
        };
        Timer timer = new Timer();
        // 5 seconds ought to be enough for anybody
        timer.schedule(releaseLockTask, 5 * 1000);
    }

    @WrapForJNI
    private static native void notifyAlarmFired();
}
