/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class TabsClosedEventsObserverTest {
    private val device123 = Device(
        id = "123",
        displayName = "Charcoal",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    private val device1234 = Device(
        id = "1234",
        displayName = "Emerald",
        deviceType = DeviceType.MOBILE,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    private val device12345 = Device(
        id = "12345",
        displayName = "Sapphire",
        deviceType = DeviceType.MOBILE,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    @Test
    fun `GIVEN a tabs closed command WHEN the observer is notified THEN the callback is invoked`() {
        val callback: (Device?, List<String>) -> Unit = mock()
        val observer = TabsClosedEventsObserver(callback)
        val events = listOf(
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    null,
                    listOf("https://mozilla.org"),
                ),
            ),
        )

        observer.onEvents(events)

        verify(callback).invoke(eq(null), eq(listOf("https://mozilla.org")))
    }

    @Test
    fun `GIVEN a tabs closed command from a device WHEN the observer is notified THEN the callback is invoked`() {
        val callback: (Device?, List<String>) -> Unit = mock()
        val observer = TabsClosedEventsObserver(callback)
        val events = listOf(
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://mozilla.org"),
                ),
            ),
        )

        observer.onEvents(events)

        verify(callback).invoke(eq(device123), eq(listOf("https://mozilla.org")))
    }

    @Test
    fun `GIVEN multiple commands WHEN the observer is notified THEN the callback is only invoked for the tabs closed commands`() {
        val callback: (Device?, List<String>) -> Unit = mock()
        val observer = TabsClosedEventsObserver(callback)
        val events = listOf(
            AccountEvent.ProfileUpdated,
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://mozilla.org"),
                ),
            ),
        )

        observer.onEvents(events)

        verify(callback, times(1)).invoke(eq(device123), eq(listOf("https://mozilla.org")))
    }

    @Test
    fun `GIVEN multiple tabs closed commands from the same device WHEN the observer is notified THEN the callback is invoked once`() {
        val callback: (Device?, List<String>) -> Unit = mock()
        val observer = TabsClosedEventsObserver(callback)
        val events = listOf(
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://mozilla.org", "https://getfirefox.com"),
                ),
            ),
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://example.org"),
                ),
            ),
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://getthunderbird.com"),
                ),
            ),
        )

        observer.onEvents(events)

        verify(callback, times(1)).invoke(
            eq(device123),
            eq(
                listOf(
                    "https://mozilla.org",
                    "https://getfirefox.com",
                    "https://example.org",
                    "https://getthunderbird.com",
                ),
            ),
        )
    }

    @Test
    fun `GIVEN multiple tabs closed commands from different devices WHEN the observer is notified THEN the callback is invoked once per device`() {
        val callback: (Device?, List<String>) -> Unit = mock()
        val observer = TabsClosedEventsObserver(callback)
        val events = listOf(
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    null,
                    listOf("https://mozilla.org"),
                ),
            ),
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device123,
                    listOf("https://mozilla.org"),
                ),
            ),
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device1234,
                    listOf("https://mozilla.org"),
                ),
            ),
            AccountEvent.DeviceCommandIncoming(
                command = DeviceCommandIncoming.TabsClosed(
                    device12345,
                    listOf("https://mozilla.org"),
                ),
            ),
        )

        observer.onEvents(events)

        verify(callback, times(4)).invoke(any(), eq(listOf("https://mozilla.org")))
    }
}
