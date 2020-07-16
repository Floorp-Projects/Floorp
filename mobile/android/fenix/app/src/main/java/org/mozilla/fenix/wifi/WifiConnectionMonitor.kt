/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import android.app.Application
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

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
class WifiConnectionMonitor(
    private val connectivityManager: ConnectivityManager
) : Observable<WifiConnectionMonitor.Observer> by ObserverRegistry() {

    private var callbackReceived: Boolean = false
    private var isRegistered = false

    private val frameworkListener = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network?) {
            notifyAtLeastOneObserver { onWifiConnectionChanged(connected = false) }
            callbackReceived = true
        }

        override fun onAvailable(network: Network?) {
            notifyAtLeastOneObserver { onWifiConnectionChanged(connected = true) }
            callbackReceived = true
        }
    }

    /**
     * Attaches the [WifiConnectionMonitor] to the application. After this has been called, callbacks
     * added via [register] will be called until either the app exits, or [stop] is called.
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
        if (!callbackReceived) {
            notifyAtLeastOneObserver { onWifiConnectionChanged(connected = false) }
        }

        connectivityManager.registerNetworkCallback(request, frameworkListener)
        isRegistered = true
    }

    /**
     * Detatches the [WifiConnectionMonitor] from the app. No callbacks added via
     * [register] will be called after this has been called.
     */
    fun stop() {
        // Framework code will throw if an unregistered listener attempts to unregister.
        if (!isRegistered) return
        connectivityManager.unregisterNetworkCallback(frameworkListener)
        isRegistered = false
    }

    interface Observer {
        fun onWifiConnectionChanged(connected: Boolean)
    }
}
