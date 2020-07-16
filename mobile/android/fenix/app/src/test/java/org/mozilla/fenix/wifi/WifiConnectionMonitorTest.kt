/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import android.net.ConnectivityManager
import android.net.NetworkRequest
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkConstructor
import io.mockk.slot
import io.mockk.unmockkConstructor
import io.mockk.verify
import org.junit.After
import org.junit.Before
import org.junit.Test

class WifiConnectionMonitorTest {

    private lateinit var connectivityManager: ConnectivityManager
    private lateinit var wifiConnectionMonitor: WifiConnectionMonitor

    @Before
    fun setup() {
        mockkConstructor(NetworkRequest.Builder::class)
        connectivityManager = mockk(relaxUnitFun = true)
        wifiConnectionMonitor = WifiConnectionMonitor(connectivityManager)

        every {
            anyConstructed<NetworkRequest.Builder>().addTransportType(any())
        } answers { self as NetworkRequest.Builder }
    }

    @After
    fun teardown() {
        unmockkConstructor(NetworkRequest.Builder::class)
    }

    @Test
    fun `start runs only once`() {
        wifiConnectionMonitor.start()
        wifiConnectionMonitor.start()

        verify(exactly = 1) {
            connectivityManager.registerNetworkCallback(any(), any<ConnectivityManager.NetworkCallback>())
        }
    }

    @Test
    fun `stop only runs after start`() {
        wifiConnectionMonitor.stop()
        verify(exactly = 0) {
            connectivityManager.unregisterNetworkCallback(any<ConnectivityManager.NetworkCallback>())
        }

        wifiConnectionMonitor.start()
        wifiConnectionMonitor.stop()
        verify {
            connectivityManager.unregisterNetworkCallback(any<ConnectivityManager.NetworkCallback>())
        }
    }

    @Test
    fun `passes results from connectivity manager to observers`() {
        val slot = slot<ConnectivityManager.NetworkCallback>()
        every { connectivityManager.registerNetworkCallback(any(), capture(slot)) } just Runs

        wifiConnectionMonitor.start()

        // Immediately notifies observer when registered
        val observer = mockk<WifiConnectionMonitor.Observer>(relaxed = true)
        wifiConnectionMonitor.register(observer)
        verify { observer.onWifiConnectionChanged(connected = false) }

        // Notifies observer when network is available or lost
        slot.captured.onAvailable(mockk())
        verify { observer.onWifiConnectionChanged(connected = true) }

        slot.captured.onLost(mockk())
        verify { observer.onWifiConnectionChanged(connected = false) }
    }

    private fun captureNetworkCallback(): ConnectivityManager.NetworkCallback {
        val slot = slot<ConnectivityManager.NetworkCallback>()
        verify { connectivityManager.registerNetworkCallback(any(), capture(slot)) }
        return slot.captured
    }
}
