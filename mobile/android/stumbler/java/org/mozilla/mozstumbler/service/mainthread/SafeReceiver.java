/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.mainthread;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

/**
 * Responsible for registering LocalPreferenceReceiver as a receiver with LocalBroadcastManager.
 * This receiver is registered in the AndroidManifest.xml
 */
public class SafeReceiver extends BroadcastReceiver {
    static final String LOG_TAG = "StumblerSafeReceiver";
    static final String PREFERENCE_INTENT_FILTER = "STUMBLER_PREF";

    private boolean registeredLocalReceiver = false;

    @Override
    public synchronized void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }

        if (registeredLocalReceiver) {
            return;
        }

        LocalBroadcastManager.getInstance(context).registerReceiver(
                new LocalPreferenceReceiver(),
                new IntentFilter(PREFERENCE_INTENT_FILTER)
        );

        Log.d(LOG_TAG, "Registered local preference listener");

        registeredLocalReceiver = true;
    }
}
