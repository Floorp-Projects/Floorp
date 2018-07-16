/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.mainthread;
import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.stumblerthread.StumblerService;

/**
 * Starts the StumblerService, an Intent service, which by definition runs on its own thread.
 * Registered as a local broadcast receiver in SafeReceiver.
 * Starts the StumblerService in passive listening mode.
 *
 * The received intent contains enabled state, upload API key and user agent,
 * and is used to initialize the StumblerService.
 */
public class LocalPreferenceReceiver extends BroadcastReceiver {
    // This allows global debugging logs to be enabled by doing
    // |adb shell setprop log.tag.PassiveStumbler DEBUG|
    static final String LOG_TAG = "PassiveStumbler";

    @SuppressLint("NewApi")
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) {
            return;
        }

        // This value is cached, so if |setprop| is performed (as described on the LOG_TAG above),
        // then the start/stop intent must be resent by toggling the setting or stopping/starting Fennec.
        // This does not guard against dumping PII (PII in stumbler is location, wifi BSSID, cell tower details).
        AppGlobals.isDebug = Log.isLoggable(LOG_TAG, Log.DEBUG);

        StumblerService.sFirefoxStumblingEnabled.set(intent.getBooleanExtra("enabled", false));

        if (!StumblerService.sFirefoxStumblingEnabled.get()) {
            Log.d(LOG_TAG, "Stopping StumblerService | isDebug:" + AppGlobals.isDebug);
            // This calls the service's onDestroy(), and the service's onHandleIntent(...) is not called
            context.stopService(new Intent(context, StumblerService.class));
            // For testing service messages were received
            context.sendBroadcast(new Intent(AppGlobals.ACTION_TEST_SETTING_DISABLED));
            return;
        }

        // For testing service messages were received
        context.sendBroadcast(new Intent(AppGlobals.ACTION_TEST_SETTING_ENABLED));

        Log.d(LOG_TAG, "Sending passive start message | isDebug:" + AppGlobals.isDebug);

        final Intent startServiceIntent = new Intent(context, StumblerService.class);

        startServiceIntent.putExtra(StumblerService.ACTION_START_PASSIVE, true);
        startServiceIntent.putExtra(
                StumblerService.ACTION_EXTRA_MOZ_API_KEY,
                intent.getStringExtra("moz_mozilla_api_key")
        );
        startServiceIntent.putExtra(
                StumblerService.ACTION_EXTRA_USER_AGENT,
                intent.getStringExtra("user_agent")
        );

        intent.setClass(context, StumblerService.class);
        if (AppConstants.Versions.preO) {
            context.startService(intent);
        } else {
            context.startForegroundService(intent);
        }
    }
}

