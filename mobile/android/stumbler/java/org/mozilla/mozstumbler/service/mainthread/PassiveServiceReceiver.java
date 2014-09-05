/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.mainthread;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.stumblerthread.StumblerService;

/* Starts the StumblerService, an Intent service, which by definition runs on its own thread.
 * Registered as a receiver in the AndroidManifest.xml.
 * Starts the StumblerService in passive listening mode.
 *
 * The received intent serves these purposes:
 * 1) The host application enables (or disables) the StumblerService.
 *    Enabling requires passing in the upload API key. Both the enabled state, and the API key are stored in prefs.
 *
 * 2) If previously enabled by (1), notify the service to start (such as with a BOOT Intent).
 *    The StumblerService is where the enabled state is checked, and if not enabled, the
 *    service stops immediately.
 *
 * 3) Upload notification: onReceive intents are used to tell the StumblerService to check for upload.
 *    In the Fennec host app use, startup and pause are used as indicators to the StumblerService that now
 *    is a good time to try upload, as it is likely that the network is in use.
 */
public class PassiveServiceReceiver extends BroadcastReceiver {
    static final String LOG_TAG = AppGlobals.LOG_PREFIX + PassiveServiceReceiver.class.getSimpleName();

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }

        final String action = intent.getAction();
        final boolean isIntentFromHostApp = (action != null) && action.contains(".STUMBLER_PREF");
        if (!isIntentFromHostApp) {
            Log.d(LOG_TAG, "Stumbler: received intent external to host app");
            Intent startServiceIntent = new Intent(context, StumblerService.class);
            startServiceIntent.putExtra(StumblerService.ACTION_NOT_FROM_HOST_APP, true);
            context.startService(startServiceIntent);
            return;
        }

        if (intent.hasExtra("is_debug")) {
            AppGlobals.isDebug = intent.getBooleanExtra("is_debug", false);
        }
        StumblerService.sFirefoxStumblingEnabled.set(intent.getBooleanExtra("enabled", false));

        if (!StumblerService.sFirefoxStumblingEnabled.get()) {
            // This calls the service's onDestroy(), and the service's onHandleIntent(...) is not called
            context.stopService(new Intent(context, StumblerService.class));
            return;
        }

        Log.d(LOG_TAG, "Stumbler: Sending passive start message | isDebug:" + AppGlobals.isDebug);


        final Intent startServiceIntent = new Intent(context, StumblerService.class);
        startServiceIntent.putExtra(StumblerService.ACTION_START_PASSIVE, true);
        final String mozApiKey = intent.getStringExtra("moz_mozilla_api_key");
        startServiceIntent.putExtra(StumblerService.ACTION_EXTRA_MOZ_API_KEY, mozApiKey);
        final String userAgent = intent.getStringExtra("user_agent");
        startServiceIntent.putExtra(StumblerService.ACTION_EXTRA_USER_AGENT, userAgent);
        context.startService(startServiceIntent);
    }
}

