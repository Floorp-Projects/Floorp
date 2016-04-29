/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.support.annotation.Nullable;

public class NetworkUtils {

    /**
     * Indicates whether network connectivity exists and it is possible to establish connections and pass data.
     */
    public static boolean isConnected(Context context) {
        final NetworkInfo networkInfo = getActiveNetworkInfo(context);
        return networkInfo != null &&
                networkInfo.isConnected();
    }

    public static boolean isBackgroundDataEnabled(final Context context) {
        final NetworkInfo networkInfo = getActiveNetworkInfo(context);
        return networkInfo != null &&
                networkInfo.isAvailable() &&
                networkInfo.isConnectedOrConnecting();
    }

    @Nullable
    private static NetworkInfo getActiveNetworkInfo(final Context context) {
        final ConnectivityManager connectivity = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivity == null) {
            return null;
        }
        return connectivity.getActiveNetworkInfo(); // can return null.
    }
}
