/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.os.Build

/**
* Checks for availability of network.
*
* For devices above [Build.VERSION_CODES.M] it even checks if there's internet flowing through it or not.
* */
fun ConnectivityManager.isOnline(network: Network? = null): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        getNetworkCapabilities(network ?: activeNetwork)?.let {
            it.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
                it.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
        } ?: false
    } else {
        // for devices below android M, there's no better way to get this.
        // active network info can be null if there are no active networks.
        @Suppress("Deprecation")
        activeNetworkInfo?.isConnected ?: false
    }
}
