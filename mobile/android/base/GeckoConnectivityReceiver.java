/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

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

    private static GeckoConnectivityReceiver sInstance = new GeckoConnectivityReceiver();

    private final IntentFilter mFilter;
    private Context mApplicationContext;
    private boolean mIsEnabled;

    public static GeckoConnectivityReceiver getInstance() {
        return sInstance;
    }

    private GeckoConnectivityReceiver() {
        mFilter = new IntentFilter();
        mFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
    }

    public synchronized void start(Context context) {
        if (mIsEnabled) {
            Log.w(LOGTAG, "Already started!");
            return;
        }

        mApplicationContext = context.getApplicationContext();

        // registerReceiver will return null if registering fails.
        if (mApplicationContext.registerReceiver(this, mFilter) == null) {
            Log.e(LOGTAG, "Registering receiver failed");
        } else {
            mIsEnabled = true;
        }
    }

    public synchronized void stop() {
        if (!mIsEnabled) {
            Log.w(LOGTAG, "Already stopped!");
            return;
        }

        mApplicationContext.unregisterReceiver(this);
        mApplicationContext = null;
        mIsEnabled = false;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        ConnectivityManager cm = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();

        final String status;
        if (info == null) {
            status = LINK_DATA_UNKNOWN;
        } else if (!info.isConnected()) {
            status = LINK_DATA_DOWN;
        } else {
            status = LINK_DATA_UP;
        }

        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createNetworkLinkChangeEvent(status));
        }
    }
}
