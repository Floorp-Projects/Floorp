/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import android.app.Application
import android.net.ConnectivityManager
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class WifiConnectionMonitorTest {
    lateinit var settings: Settings
    lateinit var connectivityManager: ConnectivityManager
    lateinit var wifiConnectionMonitor: WifiConnectionMonitor

    @Before
    fun setUp() {
        settings = mockk(relaxed = true)
        wifiConnectionMonitor = WifiConnectionMonitor(testContext as Application)
        connectivityManager = spyk(wifiConnectionMonitor.connectivityManager)
        wifiConnectionMonitor.connectivityManager = connectivityManager
    }

    @Test
    fun `WHEN the feature starts THEN all the network callbacks must be registered`() {
        val spyWifiConnectionMonitor = spyk(wifiConnectionMonitor)

        spyWifiConnectionMonitor.connectivityManager = connectivityManager

        spyWifiConnectionMonitor.start()

        verify(exactly = 1) {
            connectivityManager.registerNetworkCallback(
                any(),
                wifiConnectionMonitor.frameworkListener,
            )
        }

        verify(exactly = 1) {
            spyWifiConnectionMonitor.notifyListeners(false)
        }

        assertTrue(spyWifiConnectionMonitor.isRegistered)
        assertFalse(spyWifiConnectionMonitor.lastKnownStateWasAvailable!!)
    }

    @Test
    fun `WHEN the feature starts multiple times THEN the network callbacks must be registered once`() {
        wifiConnectionMonitor.isRegistered = true

        wifiConnectionMonitor.start()
        wifiConnectionMonitor.start()

        verify(exactly = 0) {
            connectivityManager.registerNetworkCallback(
                any(),
                wifiConnectionMonitor.frameworkListener,
            )
        }
    }

    @Test
    fun `WHEN the feature stops THEN the network callbacks must be unregistered`() {
        wifiConnectionMonitor.start()
        wifiConnectionMonitor.stop()

        verify {
            wifiConnectionMonitor.connectivityManager.unregisterNetworkCallback(
                wifiConnectionMonitor.frameworkListener,
            )
        }

        assertFalse(wifiConnectionMonitor.isRegistered)
        assertTrue(wifiConnectionMonitor.callbacks.isEmpty())
        assertNull(wifiConnectionMonitor.lastKnownStateWasAvailable)
    }

    @Test
    fun `WHEN the feature gets stopped multiple time THEN the network callbacks must be unregistered once`() {
        wifiConnectionMonitor.isRegistered = false

        wifiConnectionMonitor.stop()

        verify(exactly = 0) {
            connectivityManager.unregisterNetworkCallback(wifiConnectionMonitor.frameworkListener)
        }
    }

    @Test
    fun `WHEN adding a listener THEN should be added to the callback queue`() {
        wifiConnectionMonitor.addOnWifiConnectedChangedListener({})

        assertFalse(wifiConnectionMonitor.callbacks.isEmpty())
    }

    @Test
    fun `GIVEN a network status is known WHEN adding a new listener THEN the listener will be notified`() {
        var wasNotified: Boolean? = null
        wifiConnectionMonitor.lastKnownStateWasAvailable = false

        wifiConnectionMonitor.addOnWifiConnectedChangedListener {
            wasNotified = it
        }

        assertFalse(wasNotified!!)
        assertFalse(wifiConnectionMonitor.callbacks.isEmpty())
    }

    @Test
    fun `WHEN removing a listener THEN it will be removed from the listeners queue`() {
        assertTrue(wifiConnectionMonitor.callbacks.isEmpty())

        val callback: (Boolean) -> Unit = {}
        wifiConnectionMonitor.addOnWifiConnectedChangedListener(callback)

        assertFalse(wifiConnectionMonitor.callbacks.isEmpty())

        wifiConnectionMonitor.removeOnWifiConnectedChangedListener(callback)

        assertTrue(wifiConnectionMonitor.callbacks.isEmpty())
    }

    @Test
    fun `WHEN the connection is lost THEN listeners will be notified`() {
        var wasNotified: Boolean? = null

        val callback: (Boolean) -> Unit = { wasNotified = it }

        wifiConnectionMonitor.addOnWifiConnectedChangedListener(callback)
        wifiConnectionMonitor.frameworkListener.onLost(mockk())

        assertFalse(wasNotified!!)
    }

    @Test
    fun `WHEN the connection is available THEN listeners will be notified`() {
        var wasNotified: Boolean? = null

        val callback: (Boolean) -> Unit = { wasNotified = it }

        wifiConnectionMonitor.addOnWifiConnectedChangedListener(callback)
        wifiConnectionMonitor.frameworkListener.onAvailable(mockk())

        assertTrue(wasNotified!!)
    }

    @Test
    fun `GIVEN multiple listeners were added WHEN there is an update THEN all listeners must be notified`() {
        var wasNotified1: Boolean? = null
        var wasNotified2: Boolean? = null

        val callback1: (Boolean) -> Unit = { wasNotified1 = it }
        val callback2: (Boolean) -> Unit = { wasNotified2 = it }

        wifiConnectionMonitor.addOnWifiConnectedChangedListener(callback1)
        wifiConnectionMonitor.addOnWifiConnectedChangedListener(callback2)
        wifiConnectionMonitor.notifyListeners(true)

        assertTrue(wasNotified1!!)
        assertTrue(wasNotified2!!)
    }

    @Test
    fun `GIVEN multiple listeners are are added and notify THEN a ConcurrentModificationException must not be thrown`() {
        repeat(100) {
            // Adding to callbacks.
            wifiConnectionMonitor.addOnWifiConnectedChangedListener {
                // Altering callbacks while looping.
                if (wifiConnectionMonitor.callbacks.isNotEmpty()) {
                    wifiConnectionMonitor.callbacks.removeFirst()
                }
            }

            // Looping over callbacks.
            wifiConnectionMonitor.notifyListeners(true)
        }
    }
}
