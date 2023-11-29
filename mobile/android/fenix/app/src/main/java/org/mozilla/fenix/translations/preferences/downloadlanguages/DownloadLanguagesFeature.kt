/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import android.app.Application
import android.content.Context
import android.net.ConnectivityManager
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.net.ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_ENABLED
import androidx.core.net.ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_WHITELISTED
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.wifi.WifiConnectionMonitor

/**
 * Helper for observing WiFi connection and data saving mode.
 *
 * @param context Android context.
 * @param wifiConnectionMonitor Attaches itself to the [Application]
 * and listens for WIFI available/not available events.
 * @param onDataSaverAndWifiChanged A callback that will return true if the data saver is on and WiFi is off.
 */
class DownloadLanguagesFeature(
    private val context: Context,
    private val wifiConnectionMonitor: WifiConnectionMonitor,
    private val onDataSaverAndWifiChanged: (Boolean) -> Unit,
) : LifecycleAwareFeature {

    @VisibleForTesting
    internal var connectivityManager: ConnectivityManager? = null

    @VisibleForTesting
    internal val wifiConnectedListener: ((Boolean) -> Unit) by lazy {
        { connected: Boolean ->
            var isDataSaverEnabled = false

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                val restrictBackgroundStatus = connectivityManager?.restrictBackgroundStatus
                if (restrictBackgroundStatus == RESTRICT_BACKGROUND_STATUS_ENABLED ||
                    restrictBackgroundStatus == RESTRICT_BACKGROUND_STATUS_WHITELISTED
                ) {
                    isDataSaverEnabled = true
                }
            }

            if (isDataSaverEnabled && !connected) {
                onDataSaverAndWifiChanged.invoke(true)
            } else {
                onDataSaverAndWifiChanged.invoke(false)
            }
        }
    }

    private fun addWifiConnectedListener() {
        wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    private fun removeWifiConnectedListener() {
        wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    override fun start() {
        connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as
            ConnectivityManager
        wifiConnectionMonitor.start()
        addWifiConnectedListener()
    }

    override fun stop() {
        connectivityManager = null
        wifiConnectionMonitor.stop()
        removeWifiConnectedListener()
    }
}
