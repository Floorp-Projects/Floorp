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
    private static final String LOG_TAG = AppGlobals.LOG_PREFIX + NetworkUtils.class.getSimpleName();

    ConnectivityManager mConnectivityManager;
    static NetworkUtils sInstance;

    /* Created at startup by app, or service, using a context. */
    static public void createGlobalInstance(Context context) {
        sInstance = new NetworkUtils();
        sInstance.mConnectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    /* If accessed before singleton instantiation will abort. */
    public static NetworkUtils getInstance() {
        assert(sInstance != null);
        return sInstance;
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
