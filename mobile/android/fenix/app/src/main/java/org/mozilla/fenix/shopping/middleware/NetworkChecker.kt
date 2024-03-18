/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import android.content.Context
import android.net.ConnectivityManager
import androidx.core.content.getSystemService
import org.mozilla.fenix.ext.isOnline

/**
 * Checks if the device is connected to the internet.
 */
interface NetworkChecker {

    /**
     * @return true if the device is connected to the internet, false otherwise.
     */
    fun isConnected(): Boolean
}

/**
 * @see [NetworkChecker].
 */
class DefaultNetworkChecker(private val context: Context) : NetworkChecker {

    private val connectivityManager by lazy { context.getSystemService<ConnectivityManager>() }

    override fun isConnected(): Boolean {
        return connectivityManager?.isOnline() ?: false
    }
}
