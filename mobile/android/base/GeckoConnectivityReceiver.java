/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.Log;

public class GeckoConnectivityReceiver extends BroadcastReceiver {
    /*
     * Keep the below constants in sync with
     * http://mxr.mozilla.org/mozilla-central/source/netwerk/base/public/nsINetworkLinkService.idl
     */
    private static final String LINK_DATA_UP = "up";
    private static final String LINK_DATA_DOWN = "down";
    private static final String LINK_DATA_UNKNOWN = "unknown";

    private static final String LOGTAG = "GeckoConnectivityReceiver";

    private IntentFilter mFilter;

    private static boolean isRegistered = false;

    public GeckoConnectivityReceiver() {
        mFilter = new IntentFilter();
        mFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String status;
        ConnectivityManager cm = (ConnectivityManager)
            context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null)
            status = LINK_DATA_UNKNOWN;
        else if (!info.isConnected())
            status = LINK_DATA_DOWN;
        else
            status = LINK_DATA_UP;

        if (GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning))
            GeckoAppShell.onChangeNetworkLinkStatus(status);
    }

    public void registerFor(Activity activity) {
        if (!isRegistered) {
            // registerReciever will return null if registering fails
            isRegistered = activity.registerReceiver(this, mFilter) != null;
            if (!isRegistered)
                Log.e(LOGTAG, "Registering receiver failed");
        }
    }

    public void unregisterFor(Activity activity) {
        if (isRegistered) {
            try {
                activity.unregisterReceiver(this);
            } catch (IllegalArgumentException iae) {
                Log.e(LOGTAG, "Unregistering receiver failed", iae);
            }
            isRegistered = false;
        }
    }
}
