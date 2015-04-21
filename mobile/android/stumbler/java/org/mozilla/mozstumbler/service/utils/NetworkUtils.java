/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.utils;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.Log;
import org.mozilla.mozstumbler.service.AppGlobals;

public final class NetworkUtils {
    private static final String LOG_TAG = AppGlobals.makeLogTag(NetworkUtils.class.getSimpleName());

    ConnectivityManager mConnectivityManager;

    public NetworkUtils(Context context) {
        mConnectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    public synchronized boolean isWifiAvailable() {
        if (mConnectivityManager == null) {
            Log.e(LOG_TAG, "ConnectivityManager is null!");
            return false;
        }

        NetworkInfo aNet = mConnectivityManager.getActiveNetworkInfo();
        return (aNet != null && aNet.getType() == ConnectivityManager.TYPE_WIFI);
    }

}
