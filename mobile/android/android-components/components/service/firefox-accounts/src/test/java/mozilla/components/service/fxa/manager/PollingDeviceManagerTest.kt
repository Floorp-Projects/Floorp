/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.setMain
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.TabData
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.util.concurrent.Executors

class PollingDeviceManagerTest {

    internal class TestableDeviceManager(
        constellation: DeviceConstellation,
        scope: CoroutineScope,
        registry: ObserverRegistry<DeviceConstellationObserver> = ObserverRegistry(),
        val block: () -> PeriodicRefreshManager
    ) : PollingDeviceManager(constellation, scope, registry) {
        override fun getRefreshManager(): PeriodicRefreshManager {
            return block()
        }
    }

    @Test
    fun `starting and stopping refresh`() = runBlocking {
        val refreshManager: PeriodicRefreshManager = mock()
        val manager = TestableDeviceManager(mock(), this) { refreshManager }
        verify(refreshManager, never()).startRefresh()
        verify(refreshManager, never()).stopRefresh()

        manager.startPolling()
        verify(refreshManager).startRefresh()
        verify(refreshManager, never()).stopRefresh()

        manager.stopPolling()
        verify(refreshManager).startRefresh()
        verify(refreshManager).stopRefresh()
    }

    @Test
    fun `polling for events triggers observers`() = runBlocking {
        val constellation: DeviceConstellation = mock()
        val manager = TestableDeviceManager(constellation, this) { mock() }

        // No events, no observers.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf()))
        assertTrue(manager.pollAsync().await())

        val eventsObserver = object : DeviceEventsObserver {
            var latestEvents: List<DeviceEvent>? = null

            override fun onEvents(events: List<DeviceEvent>) {
                latestEvents = events
            }
        }

        // No events, with an observer.
        manager.register(eventsObserver)
        assertTrue(manager.pollAsync().await())
        assertEquals(listOf<DeviceEvent>(), eventsObserver.latestEvents)

        // Some events.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf(
            DeviceEvent.TabReceived(null, listOf())
        )))
        assertTrue(manager.pollAsync().await())

        var events = eventsObserver.latestEvents!!
        assertEquals(null, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf<TabData>(), (events[0] as DeviceEvent.TabReceived).entries)

        val testDevice1 = testDevice("test1", false)
        val testDevice2 = testDevice("test2", false)
        val testTab1 = TabData("Hello", "http://world.com/1")
        val testTab2 = TabData("Hello", "http://world.com/2")
        val testTab3 = TabData("Hello", "http://world.com/3")

        // Zero tabs from a single device.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf(
            DeviceEvent.TabReceived(testDevice1, listOf())
        )))
        assertTrue(manager.pollAsync().await())
        assertNotNull(eventsObserver.latestEvents)
        assertEquals(1, eventsObserver.latestEvents!!.size)
        events = eventsObserver.latestEvents!!
        assertEquals(testDevice1, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf<TabData>(), (events[0] as DeviceEvent.TabReceived).entries)

        // Single tab from a single device.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf(
            DeviceEvent.TabReceived(testDevice2, listOf(testTab1))
        )))
        assertTrue(manager.pollAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1), (events[0] as DeviceEvent.TabReceived).entries)

        // Multiple tabs from a single device.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf(
                DeviceEvent.TabReceived(testDevice2, listOf(testTab1, testTab3))
        )))
        assertTrue(manager.pollAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1, testTab3), (events[0] as DeviceEvent.TabReceived).entries)

        // Multiple tabs received from multiple devices.
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(listOf(
            DeviceEvent.TabReceived(testDevice2, listOf(testTab1, testTab2)),
            DeviceEvent.TabReceived(testDevice1, listOf(testTab3))
        )))
        assertTrue(manager.pollAsync().await())

        events = eventsObserver.latestEvents!!
        assertEquals(testDevice2, (events[0] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab1, testTab2), (events[0] as DeviceEvent.TabReceived).entries)
        assertEquals(testDevice1, (events[1] as DeviceEvent.TabReceived).from)
        assertEquals(listOf(testTab3), (events[1] as DeviceEvent.TabReceived).entries)

        // Failure to poll for events
        `when`(constellation.pollForEventsAsync()).thenReturn(CompletableDeferred(value = null))
        assertFalse(manager.pollAsync().await())
    }

    @Test
    @ExperimentalCoroutinesApi
    fun `refreshing devices triggers observers`() {
        val testDispatcher = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
        val testScope = CoroutineScope(testDispatcher)
        Dispatchers.setMain(testDispatcher)

        val constellation: DeviceConstellation = mock()
        val registry = ObserverRegistry<DeviceConstellationObserver>()
        val manager = TestableDeviceManager(constellation, testScope, registry) { mock() }

        // No devices, no observers.
        `when`(constellation.fetchAllDevicesAsync()).thenReturn(CompletableDeferred(listOf()))

        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }

        val observer = object : DeviceConstellationObserver {
            var state: ConstellationState? = null

            override fun onDevicesUpdate(constellation: ConstellationState) {
                state = constellation
            }
        }
        registry.register(observer)

        // No devices, with an observer.
        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }
        assertEquals(ConstellationState(null, listOf()), observer.state)

        val testDevice1 = testDevice("test1", false)
        val testDevice2 = testDevice("test2", false)
        val currentDevice = testDevice("currentTestDevice", true)

        // Single device, no current device.
        `when`(constellation.fetchAllDevicesAsync()).thenReturn(CompletableDeferred(listOf(
            testDevice1
        )))
        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }
        assertEquals(ConstellationState(null, listOf(testDevice1)), observer.state)
        assertEquals(ConstellationState(null, listOf(testDevice1)), manager.constellationState())

        // Current device, no other devices.
        `when`(constellation.fetchAllDevicesAsync()).thenReturn(CompletableDeferred(listOf(
                currentDevice
        )))
        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }
        assertEquals(ConstellationState(currentDevice, listOf()), observer.state)
        assertEquals(ConstellationState(currentDevice, listOf()), manager.constellationState())

        // Current device with other devices.
        `when`(constellation.fetchAllDevicesAsync()).thenReturn(CompletableDeferred(listOf(
                currentDevice, testDevice1, testDevice2
        )))
        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }
        assertEquals(ConstellationState(currentDevice, listOf(testDevice1, testDevice2)), observer.state)
        assertEquals(ConstellationState(currentDevice, listOf(testDevice1, testDevice2)), manager.constellationState())

        // Current device with expired subscription.
        val currentDeviceExpired = testDevice("currentExpired", true, expired = true)
        `when`(constellation.fetchAllDevicesAsync()).thenReturn(CompletableDeferred(listOf(
                currentDeviceExpired, testDevice2
        )))
        runBlocking(testDispatcher) {
            manager.refreshDevicesAsync().await()
        }
        assertEquals(ConstellationState(currentDeviceExpired, listOf(testDevice2)), observer.state)
        assertEquals(ConstellationState(currentDeviceExpired, listOf(testDevice2)), manager.constellationState())
    }

    private fun testDevice(id: String, current: Boolean, expired: Boolean = false): Device {
        return Device(
            id = id,
            displayName = "testName",
            deviceType = DeviceType.MOBILE,
            isCurrentDevice = current,
            lastAccessTime = 123L,
            capabilities = listOf(),
            subscriptionExpired = expired,
            subscription = null
        )
    }
}