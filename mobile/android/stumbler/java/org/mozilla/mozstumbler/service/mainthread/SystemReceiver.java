/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.mainthread;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.mozstumbler.service.stumblerthread.StumblerService;

/**
 * Responsible for starting StumblerService in response to
 * BOOT_COMPLETE and EXTERNAL_APPLICATIONS_AVAILABLE system intents.
 */
public class SystemReceiver extends BroadcastReceiver {
    static final String LOG_TAG = "StumblerSystemReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }

        final String action = intent.getAction();

        if (!TextUtils.equals(action, Intent.ACTION_BOOT_COMPLETED) && !TextUtils.equals(action, Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE)) {
            // This is not the broadcast you are looking for.
            return;
        }

        final Intent startServiceIntent = new Intent(context, StumblerService.class);
        startServiceIntent.putExtra(StumblerService.ACTION_NOT_FROM_HOST_APP, true);
        context.startService(startServiceIntent);

        Log.d(LOG_TAG, "Responded to a system intent");
    }
}
