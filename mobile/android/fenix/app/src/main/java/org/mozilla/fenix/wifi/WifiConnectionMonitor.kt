/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import android.app.Application
import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import androidx.annotation.VisibleForTesting

/**
 * Attaches itself to the [Application] and listens for WIFI available/not available events. This
 * allows calling code to set simpler listeners.
 *
 * Example:
 * ```kotlin
 *  app.components.wifiConnectionListener.addOnWifiConnectedChangedListener { isConnected ->
 *    if (isConnected) {
 *      downloadThing()
 *    }
 *  }
 *  app.components.wifiConnectionListener.start()
 * ```
 */
class WifiConnectionMonitor(app: Application) {
    @VisibleForTesting
    internal val callbacks = mutableListOf<(Boolean) -> Unit>()

    @VisibleForTesting
    internal var connectivityManager = app.getSystemService(Context.CONNECTIVITY_SERVICE) as
        ConnectivityManager

    @VisibleForTesting
    internal var lastKnownStateWasAvailable: Boolean? = null

    @VisibleForTesting
    internal var isRegistered = false
        @Synchronized set

    @VisibleForTesting
    internal val frameworkListener = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) {
            notifyListeners(false)
            lastKnownStateWasAvailable = false
        }

        override fun onAvailable(network: Network) {
            notifyListeners(true)
            lastKnownStateWasAvailable = true
        }
    }

    internal fun notifyListeners(value: Boolean) {
        val items = ArrayList(callbacks)
        items.forEach { it(value) }
    }

    /**
     * Attaches the [WifiConnectionMonitor] to the application. After this has been called, callbacks
     * added via [addOnWifiConnectedChangedListener] will be called until either the app exits, or
     * [stop] is called.
     *
     * Any existing callbacks will be called with the current state when this is called.
     */
    fun start() {
        // Framework code throws if a listener is registered twice without unregistering.
        if (isRegistered) return
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()

        // AFAICT, the framework does not send an event when a new NetworkCallback is registered
        // while the WIFI is not connected, so we push this manually. If the WIFI is on, it will send
        // a follow up event shortly
        val noCallbacksReceivedYet = lastKnownStateWasAvailable == null
        if (noCallbacksReceivedYet) {
            lastKnownStateWasAvailable = false
            notifyListeners(false)
        }

        connectivityManager.registerNetworkCallback(request, frameworkListener)
        isRegistered = true
    }

    /**
     * Detatches the [WifiConnectionMonitor] from the app. No callbacks added via
     * [addOnWifiConnectedChangedListener] will be called after this has been called.
     */
    fun stop() {
        // Framework code will throw if an unregistered listener attempts to unregister.
        if (!isRegistered) return
        connectivityManager.unregisterNetworkCallback(frameworkListener)
        isRegistered = false
        lastKnownStateWasAvailable = null
        callbacks.clear()
    }

    /**
     * Adds [onWifiChanged] to a list of listeners that will be called whenever WIFI connects or
     * disconnects.
     *
     * If [onWifiChanged] is successfully added (i.e., it is a new listener), it will be immediately
     * called with the last known state.
     */
    fun addOnWifiConnectedChangedListener(onWifiChanged: (Boolean) -> Unit) {
        val lastKnownState = lastKnownStateWasAvailable
        if (callbacks.add(onWifiChanged) && lastKnownState != null) {
            onWifiChanged(lastKnownState)
        }
    }

    /**
     * Removes [onWifiChanged] from the list of listeners to be called whenever WIFI connects or
     * disconnects.
     */
    fun removeOnWifiConnectedChangedListener(onWifiChanged: (Boolean) -> Unit) {
        callbacks.remove(onWifiChanged)
    }
}
