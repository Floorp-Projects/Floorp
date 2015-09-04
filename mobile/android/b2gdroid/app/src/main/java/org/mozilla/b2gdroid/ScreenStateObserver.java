/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.b2gdroid;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;

import org.json.JSONObject;
import org.json.JSONArray;
import org.json.JSONException;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;

class ScreenStateObserver extends BroadcastReceiver {
        private static final String LOGTAG = "B2G";

    ScreenStateObserver(Context context) {
        Log.d(LOGTAG, "ScreenStateObserver constructor");
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        context.registerReceiver(this, filter);
    }

    void destroy(Context context) {
        Log.d(LOGTAG, "ScreenStateObserver::unint");
        context.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(LOGTAG, "ScreenStateObserver: " + intent.getAction());
        String action = "no_op";
        switch(intent.getAction()) {
            case "android.intent.action.SCREEN_ON":
                action = "screen_on";
                break;
            case "android.intent.action.SCREEN_OFF":
                action = "screen_off";
                break;
        }
        JSONObject obj = new JSONObject();
        try {
            obj.put("action", action);
        } catch(JSONException ex) {
            Log.wtf(LOGTAG, "Error building Android:Launcher message", ex);
        }
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Android:Launcher", obj.toString());
        GeckoAppShell.sendEventToGecko(e);
    }
}
