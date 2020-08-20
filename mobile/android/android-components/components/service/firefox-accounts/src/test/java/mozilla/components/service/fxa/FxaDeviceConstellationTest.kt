/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.plus
import kotlinx.coroutines.runBlocking
import mozilla.appservices.fxaclient.AccountEvent as ASAccountEvent // the app-services variation
import mozilla.appservices.fxaclient.IncomingDeviceCommand
import mozilla.appservices.fxaclient.TabHistoryEntry
import mozilla.appservices.syncmanager.DeviceSettings
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.TabData
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import mozilla.appservices.fxaclient.Device as NativeDevice
import mozilla.appservices.fxaclient.FirefoxAccount as NativeFirefoxAccount
import mozilla.appservices.syncmanager.DeviceType as RustDeviceType

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class FxaDeviceConstellationTest {
    lateinit var account: NativeFirefoxAccount
    lateinit var constellation: FxaDeviceConstellation

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        account = mock()
        val scope = CoroutineScope(coroutinesTestRule.testDispatcher) + SupervisorJob()
        constellation = FxaDeviceConstellation(account, scope)
    }

    @Test
    fun `initializing device`() = runBlocking(coroutinesTestRule.testDispatcher) {
        constellation.initDeviceAsync("test name", DeviceType.TABLET, setOf()).await()
        verify(account).initializeDevice("test name", NativeDevice.Type.TABLET, setOf())

        constellation.initDeviceAsync("VR device", DeviceType.VR, setOf(DeviceCapability.SEND_TAB)).await()
        verify(account).initializeDevice("VR device", NativeDevice.Type.VR, setOf(mozilla.appservices.fxaclient.Device.Capability.SEND_TAB))
    }

    @Test
    fun `ensure capabilities`() = runBlocking(coroutinesTestRule.testDispatcher) {
        constellation.ensureCapabilitiesAsync(setOf()).await()
        verify(account).ensureCapabilities(setOf())

        constellation.ensureCapabilitiesAsync(setOf(DeviceCapability.SEND_TAB)).await()
        verify(account).ensureCapabilities(setOf(mozilla.appservices.fxaclient.Device.Capability.SEND_TAB))
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `updating device name`() = runBlocking(coroutinesTestRule.testDispatcher) {
        val currentDevice = testDevice("currentTestDevice", true)
        `when`(account.getDevices()).thenReturn(arrayOf(currentDevice))

        // Can't update cached value in an empty cache
        try {
            constellation.setDeviceNameAsync("new name", testContext).await()
            fail()
        } catch (e: IllegalStateException) {}

        val cache = FxaDeviceSettingsCache(testContext)
        cache.setToCache(DeviceSettings("someId", "test name", RustDeviceType.MOBILE))

        // No device state observer.
        assertTrue(constellation.setDeviceNameAsync("new name", testContext).await())
        verify(account, times(2)).setDeviceDisplayName("new name")

        assertEquals(DeviceSettings("someId", "new name", RustDeviceType.MOBILE), cache.getCached())

        // Set up the observer...
        val observer = object : DeviceConstellationObserver {
            var state: ConstellationState? = null

            override fun onDevicesUpdate(constellation: ConstellationState) {
                state = constellation
            }
        }
        constellation.registerDeviceObserver(observer, startedLifecycleOwner(), false)

        assertTrue(constellation.setDeviceNameAsync("another name", testContext).await())
        verify(account).setDeviceDisplayName("another name")

        assertEquals(DeviceSettings("someId", "another name", RustDeviceType.MOBILE), cache.getCached())

        // Since we're faking the data, here we're just testing that observer is notified with the
        // up-to-date constellation.
        assertEquals(observer.state!!.currentDevice!!.displayName, "testName")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `set device push subscription`() = runBlocking(coroutinesTestRule.testDispatcher) {
        val subscription = DevicePushSubscription("http://endpoint.com", "pk", "auth key")
        constellation.setDevicePushSubscriptionAsync(subscription).await()

        verify(account).setDevicePushSubscription("http://endpoint.com", "pk", "auth key")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `process raw device command`() = runBlocking(coroutinesTestRule.testDispatcher) {
        // No commands, no observer.
        `when`(account.handlePushMessage("raw events payload")).thenReturn(emptyArray())
        assertTrue(constellation.processRawEventAsync("raw events payload").await())

        // No commands, with observer.
        val eventsObserver = object : AccountEventsObserver {
            var latestEvents: List<AccountEvent>? = null

            override fun onEvents(events: List<AccountEvent>) {
                latestEvents = events
            }
        }

        // No commands, with an observer.
        constellation.register(eventsObserver)
        assertTrue(constellation.processRawEventAsync("raw events payload").await())
        assertEquals(listOf<AccountEvent.DeviceCommandIncoming>(), eventsObserver.latestEvents)

        // Some commands, with an observer. More detailed command handling tests below.
        val testDevice1 = testDevice("test1", false)
        val testTab1 = TabHistoryEntry("Hello", "http://world.com/1")
        `when`(account.handlePushMessage("raw events payload")).thenReturn(arrayOf(
            ASAccountEvent.IncomingDeviceCommand(
                command = IncomingDeviceCommand.TabReceived(testDevice1, arrayOf(testTab1))
            )
        ))
        assertTrue(constellation.processRawEventAsync("raw events payload").await())

        val events = eventsObserver.latestEvents!!
        val command = (events[0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice1.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf(testTab1.into()), command.entries)
    }

    @Test
    fun `send command to device`() = runBlocking(coroutinesTestRule.testDispatcher) {
        `when`(account.gatherTelemetry()).thenReturn("{}")
        assertTrue(constellation.sendCommandToDeviceAsync(
            "targetID", DeviceCommandOutgoing.SendTab("Mozilla", "https://www.mozilla.org")
        ).await())

        verify(account).sendSingleTab("targetID", "Mozilla", "https://www.mozilla.org")
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `refreshing constellation`() = runBlocking(coroutinesTestRule.testDispatcher) {
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
    fun `polling for commands triggers observers`() = runBlocking(coroutinesTestRule.testDispatcher) {
        // No commands, no observers.
        `when`(account.gatherTelemetry()).thenReturn("{}")
        `when`(account.pollDeviceCommands()).thenReturn(emptyArray())
        assertTrue(constellation.pollForCommandsAsync().await())

        val eventsObserver = object : AccountEventsObserver {
            var latestEvents: List<AccountEvent>? = null

            override fun onEvents(events: List<AccountEvent>) {
                latestEvents = events
            }
        }

        // No commands, with an observer.
        constellation.register(eventsObserver)
        assertTrue(constellation.pollForCommandsAsync().await())
        assertEquals(listOf<AccountEvent>(), eventsObserver.latestEvents)

        // Some commands.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            IncomingDeviceCommand.TabReceived(null, emptyArray())
        ))
        assertTrue(constellation.pollForCommandsAsync().await())

        var command = (eventsObserver.latestEvents!![0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(null, (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf<TabData>(), command.entries)

        val testDevice1 = testDevice("test1", false)
        val testDevice2 = testDevice("test2", false)
        val testTab1 = TabHistoryEntry("Hello", "http://world.com/1")
        val testTab2 = TabHistoryEntry("Hello", "http://world.com/2")
        val testTab3 = TabHistoryEntry("Hello", "http://world.com/3")

        // Zero tabs from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            IncomingDeviceCommand.TabReceived(testDevice1, emptyArray())
        ))
        assertTrue(constellation.pollForCommandsAsync().await())

        Assert.assertNotNull(eventsObserver.latestEvents)
        assertEquals(1, eventsObserver.latestEvents!!.size)
        command = (eventsObserver.latestEvents!![0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice1.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf<TabData>(), command.entries)

        // Single tab from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            IncomingDeviceCommand.TabReceived(testDevice2, arrayOf(testTab1))
        ))
        assertTrue(constellation.pollForCommandsAsync().await())

        command = (eventsObserver.latestEvents!![0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice2.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf(testTab1.into()), command.entries)

        // Multiple tabs from a single device.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            IncomingDeviceCommand.TabReceived(testDevice2, arrayOf(testTab1, testTab3))
        ))
        assertTrue(constellation.pollForCommandsAsync().await())

        command = (eventsObserver.latestEvents!![0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice2.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf(testTab1.into(), testTab3.into()), command.entries)

        // Multiple tabs received from multiple devices.
        `when`(account.pollDeviceCommands()).thenReturn(arrayOf(
            IncomingDeviceCommand.TabReceived(testDevice2, arrayOf(testTab1, testTab2)),
            IncomingDeviceCommand.TabReceived(testDevice1, arrayOf(testTab3))
        ))
        assertTrue(constellation.pollForCommandsAsync().await())

        command = (eventsObserver.latestEvents!![0] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice2.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf(testTab1.into(), testTab2.into()), command.entries)
        command = (eventsObserver.latestEvents!![1] as AccountEvent.DeviceCommandIncoming).command
        assertEquals(testDevice1.into(), (command as DeviceCommandIncoming.TabReceived).from)
        assertEquals(listOf(testTab3.into()), command.entries)

        // TODO FirefoxAccount needs @Throws annotations for these tests to actually work.
        // Failure to poll for commands. Panics are re-thrown.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaPanicException("Don't panic!"))
//        try {
//            runBlocking(coroutinesTestRule.testDispatcher) {
//                constellation.refreshAsync().await()
//            }
//            fail()
//        } catch (e: FxaPanicException) {}
//
//        // Network exception are handled.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaNetworkException("four oh four"))
//        runBlocking(coroutinesTestRule.testDispatcher) {
//            Assert.assertFalse(constellation.refreshAsync().await())
//        }
//        // Unspecified exception are handled.
//        `when`(account.pollDeviceCommands()).thenThrow(FxaUnspecifiedException("hmmm..."))
//        runBlocking(coroutinesTestRule.testDispatcher) {
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
//        runBlocking(coroutinesTestRule.testDispatcher) {
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
