/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.setMain
import mozilla.appservices.fxaclient.AccountEvent
import mozilla.appservices.fxaclient.TabHistoryEntry
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventOutgoing
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.TabData
import mozilla.components.support.test.mock
import org.junit.Assert
import mozilla.appservices.fxaclient.FirefoxAccount as NativeFirefoxAccount
import mozilla.appservices.fxaclient.Device as NativeDevice
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import java.util.concurrent.Executors

class FxaDeviceConstellationTest {
    lateinit var account: NativeFirefoxAccount
    lateinit var constellation: FxaDeviceConstellation
    lateinit var testDispatcher: CoroutineDispatcher

    @Before
    fun setup() {
        testDispatcher = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
        account = mock()
        constellation = FxaDeviceConstellation(account, CoroutineScope(testDispatcher))
    }

    @Test
    fun `initializing device`() = runBlocking(testDispatcher) {
        constellation.initDeviceAsync("test name", DeviceType.TABLET, setOf()).await()
        verify(account).initializeDevice("test name", NativeDevice.Type.TABLET, setOf())

        constellation.initDeviceAsync("VR device", DeviceType.VR, setOf(DeviceCapability.SEND_TAB)).await()
        verify(account).initializeDevice("VR device", NativeDevice.Type.VR, setOf(mozilla.appservices.fxaclient.Device.Capability.SEND_TAB))
    }

    @Test
    fun `ensure capabilities`() = runBlocking(testDispatcher) {
        constellation.ensureCapabilitiesAsync(setOf()).await()
        verify(account).ensureCapabilities(setOf())

        constellation.ensureCapabilitiesAsync(setOf(DeviceCapability.SEND_TAB)).await()
        verify(account).ensureCapabilities(setOf(mozilla.appservices.fxaclient.Device.Capability.SEND_TAB))
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `updating device name`() = runBlocking(testDispatcher) {
        // Observers are called on the main thread.
        Dispatchers.setMain(testDispatcher)

        val currentDevice = testDevice("currentTestDevice", true)
        `when`(account.getDevices()).thenReturn(arrayOf(currentDevice))

        // No device state observer.
        assertTrue(constellation.setDeviceNameAsync("new name").await())
        verify(account).setDeviceDisplayName("new name")

        // Set up the observer...
        val observer = object : DeviceConstellationObserver {
            var state: ConstellationState? = null

            override fun onDevicesUpdate(constellation: ConstellationState) {
                state = constellation
            }
        }
        constellation.registerDeviceObserver(observer, startedLifecycleOwner(), false)

        assertTrue(constellation.setDeviceNameAsync("another name").await())
        verify(account).setDeviceDisplayName("another name")

        // Since we're faking the data, here we're just testing that observer is notified with the
        // up-to-date constellation.
        assertEquals(observer.state!!.currentDevice!!.displayName, "testName")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `set device push subscription`() = runBlocking(testDispatcher) {
        // Observers are called on the main thread.
        Dispatchers.setMain(testDispatcher)

        val subscription = DevicePushSubscription("http://endpoint.com", "pk", "auth key")
        constellation.setDevicePushSubscriptionAsync(subscription).await()

        verify(account).setDevicePushSubscription("http://endpoint.com", "pk", "auth key")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `process raw device event`() = runBlocking(testDispatcher) {
        // Observers are called on the main thread.
        Dispatchers.setMain(testDispatcher)

        // No events, no observer.
        `when`(account.handlePushMessage("raw events payload")).thenReturn(emptyArray())
        assertTrue(constellation.processRawEventAsync("raw events payload").await())

        // No events, with observer.
        val eventsObserver = object : DeviceEventsObserver {
            var latestEvents: List<DeviceEvent>? = null

            override fun onEvents(events: List<DeviceEvent>) {
                latestEvents = events
            }
        }

        // No events, with an observer.
        constellation.register(eventsObserver)
        assertTrue(constellation.processRawEventAsync("raw events payload").await())
        assertEquals(listOf<DeviceEvent>(), eventsObserver.latestEvents)

        // Some events, with an observer. More detailed event handling tests below.
        val testDevice1 = testDevice("test1", false)
        val testTab1 = TabHistoryEntry("Hello", "http://world.com/1")
        `when`(account.handlePushMessage("raw events payload")).thenReturn(arrayOf(
            AccountEvent.TabReceived(testDevice1, arrayOf(testTab1))
        ))
        assertTrue(constellation.processRawEventAsync("raw events payload").await())

        val events = eventsObserver.latestEvents!!
        assertEquals(testDevice1.into(), (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1.into()), (events[0] as DeviceEvent.TabReceived).entries)
    }

    @Test
    fun `send event to device`() = runBlocking(testDispatcher) {
        assertTrue(constellation.sendEventToDeviceAsync(
            "targetID", DeviceEventOutgoing.SendTab("Mozilla", "https://www.mozilla.org")
        ).await())

        verify(account).sendSingleTab("targetID", "Mozilla", "https://www.mozilla.org")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `refreshing constellation`() = runBlocking(testDispatcher) {
        // Observers are called on the main thread.
        Dispatchers.setMain(testDispatcher)

        // No devices, no observers.
        `when`(account.getDevices()).thenReturn(emptyArray())

        constellation.refreshDevicesAsync().await()

        val observer = object : DeviceConstellationObserver {
            var state: ConstellationState? = null

            override fun onDevicesUpdate(constellation: ConstellationState) {
                state = constellation
            }
        }
        constellation.registerDeviceObserver(observer, startedLifecycleOwner(), false)

        // No devices, with an observer.
        constellation.refreshDevicesAsync().await()
        assertEquals(ConstellationState(null, listOf()), observer.state)

        val testDevice1 = testDevice("test1", false)
        val testDevice2 = testDevice("test2", false)
        val currentDevice = testDevice("currentTestDevice", true)

        // Single device, no current device.
        `when`(account.getDevices()).thenReturn(arrayOf(testDevice1))
        constellation.refreshDevicesAsync().await()

        assertEquals(ConstellationState(null, listOf(testDevice1.into())), observer.state)
        assertEquals(ConstellationState(null, listOf(testDevice1.into())), constellation.state())

        // Current device, no other devices.
        `when`(account.getDevices()).thenReturn(arrayOf(currentDevice))
        constellation.refreshDevicesAsync().await()
        assertEquals(ConstellationState(currentDevice.into(), listOf()), observer.state)
        assertEquals(ConstellationState(currentDevice.into(), listOf()), constellation.state())

        // Current device with other devices.
        `when`(account.getDevices()).thenReturn(arrayOf(
            currentDevice, testDevice1, testDevice2
        ))
        constellation.refreshDevicesAsync().await()

        assertEquals(ConstellationState(currentDevice.into(), listOf(testDevice1.into(), testDevice2.into())), observer.state)
        assertEquals(ConstellationState(currentDevice.into(), listOf(testDevice1.into(), testDevice2.into())), constellation.state())

        // Current device with expired subscription.
        val currentDeviceExpired = testDevice("currentExpired", true, expired = true)
        `when`(account.getDevices()).thenReturn(arrayOf(
            currentDeviceExpired, testDevice2
        ))
        constellation.refreshDevicesAsync().await()

        assertEquals(ConstellationState(currentDeviceExpired.into(), listOf(testDevice2.into())), observer.state)
        assertEquals(ConstellationState(currentDeviceExpired.into(), listOf(testDevice2.into())), constellation.state())
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `polling for events triggers observers`() = runBlocking(testDispatcher) {
        // Observers are called on the main thread.
        Dispatchers.setMain(testDispatcher)

        // No events, no observers.
        `when`(account.pollDeviceCommands()).thenReturn(emptyArray())
        assertTrue(constellation.pollForEventsAsync().await())

        val eventsObserver = object : DeviceEventsObserver {
            var latestEvents: List<DeviceEvent>? = null

            override fun onEvents(events: List<DeviceEvent>) {
                latestEvents = events
            }
        }

        // No events, with an observer.
        constellation.register(eventsObserver)
        assertTrue(constellation.pollForEventsAsync().await())
        assertEquals(listOf<DeviceEvent>(), eventsObserver.latestEvents)

        // Some events.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            AccountEvent.TabReceived(null, emptyArray())
        ))
        assertTrue(constellation.pollForEventsAsync().await())

        var events = eventsObserver.latestEvents!!
        assertEquals(null, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf<TabData>(), (events[0] as DeviceEvent.TabReceived).entries)

        val testDevice1 = testDevice("test1", false)
        val testDevice2 = testDevice("test2", false)
        val testTab1 = TabHistoryEntry("Hello", "http://world.com/1")
        val testTab2 = TabHistoryEntry("Hello", "http://world.com/2")
        val testTab3 = TabHistoryEntry("Hello", "http://world.com/3")

        // Zero tabs from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            AccountEvent.TabReceived(testDevice1, emptyArray())
        ))
        assertTrue(constellation.pollForEventsAsync().await())

        Assert.assertNotNull(eventsObserver.latestEvents)
        assertEquals(1, eventsObserver.latestEvents!!.size)
        events = eventsObserver.latestEvents!!
        assertEquals(testDevice1.into(), (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf<TabData>(), (events[0] as DeviceEvent.TabReceived).entries)

        // Single tab from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            AccountEvent.TabReceived(testDevice2, arrayOf(testTab1))
        ))
        assertTrue(constellation.pollForEventsAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2.into(), (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1.into()), (events[0] as DeviceEvent.TabReceived).entries)

        // Multiple tabs from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            AccountEvent.TabReceived(testDevice2, arrayOf(testTab1, testTab3))
        ))
        assertTrue(constellation.pollForEventsAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2.into(), (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1.into(), testTab3.into()), (events[0] as DeviceEvent.TabReceived).entries)

        // Multiple tabs received from multiple devices.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            AccountEvent.TabReceived(testDevice2, arrayOf(testTab1, testTab2)),
            AccountEvent.TabReceived(testDevice1, arrayOf(testTab3))
        ))
        assertTrue(constellation.pollForEventsAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2.into(), (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1.into(), testTab2.into()), (events[0] as DeviceEvent.TabReceived).entries)
        assertEquals(testDevice1.into(), (events[1] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab3.into()), (events[1] as DeviceEvent.TabReceived).entries)

        // TODO FirefoxAccount needs @Throws annotations for these tests to actually work.
        // Failure to poll for events. Panics are re-thrown.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaPanicException("Don't panic!"))
//        try {
//            runBlocking(testDispatcher) {
//                constellation.refreshAsync().await()
//            }
//            fail()
//        } catch (e: FxaPanicException) {}
//
//        // Network exception are handled.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaNetworkException("four oh four"))
//        runBlocking(testDispatcher) {
//            Assert.assertFalse(constellation.refreshAsync().await())
//        }
//        // Unspecified exception are handled.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaUnspecifiedException("hmmm..."))
//        runBlocking(testDispatcher) {
//            Assert.assertFalse(constellation.refreshAsync().await())
//        }
//        // Unauthorized exception are handled.
//        val authErrorObserver = object : AuthErrorObserver {
//            var latestException: AuthException? = null
//
//            override fun onAuthErrorAsync(e: AuthException): Deferred<Unit> {
//                latestException = e
//                val r = CompletableDeferred<Unit>()
//                r.complete(Unit)
//                return r
//            }
//        }
//        authErrorRegistry.register(authErrorObserver)
//
//        val authException = FxaUnauthorizedException("oh no you didn't!")
//        `when`(account.pollDeviceCommands()).thenThrow(authException)
//        runBlocking(testDispatcher) {
//            Assert.assertFalse(constellation.refreshAsync().await())
//        }
//        assertEquals(authErrorObserver.latestException!!.cause, authException)
    }

    private fun testDevice(id: String, current: Boolean, expired: Boolean = false): NativeDevice {
        return NativeDevice(
            id = id,
            displayName = "testName",
            deviceType = NativeDevice.Type.MOBILE,
            isCurrentDevice = current,
            lastAccessTime = 123L,
            capabilities = listOf(),
            pushEndpointExpired = expired,
            pushSubscription = null
        )
    }

    private fun startedLifecycleOwner(): LifecycleOwner {
        val lifecycleOwner = mock<LifecycleOwner>()
        val lifecycle = mock<Lifecycle>()
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(lifecycleOwner.lifecycle).thenReturn(lifecycle)
        return lifecycleOwner
    }
}