/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;

public class Restarter extends Service {
    private static final String LOGTAG = "GeckoRestarter";

    private void doRestart(Intent intent) {
        final int oldProc = intent.getIntExtra("pid", -1);
        if (oldProc < 0) {
            return;
        }

        Process.killProcess(oldProc);
        Log.d(LOGTAG, "Killed " + oldProc);
        try {
            Thread.sleep(100);
        } catch (final InterruptedException e) {
        }

        final Intent restartIntent = (Intent)intent.getParcelableExtra(Intent.EXTRA_INTENT);
        restartIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                     .putExtra("didRestart", true)
                     .setClassName(getApplicationContext(),
                                   AppConstants.BROWSER_INTENT_CLASS_NAME);
        startActivity(restartIntent);
        Log.d(LOGTAG, "Launched " + restartIntent);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        doRestart(intent);
        stopSelf(startId);
        return Service.START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
