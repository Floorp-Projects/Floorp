/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.TabData
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class EventsObserverTest {
    @Test
    fun `events are delivered successfully`() {
        val callback: (Device?, List<TabData>) -> Unit = mock()
        val observer = EventsObserver(callback)
        val events = listOf(AccountEvent.DeviceCommandIncoming(command = DeviceCommandIncoming.TabReceived(mock(), mock())))

        observer.onEvents(events)

        verify(callback).invoke(any(), any())

        observer.onEvents(listOf(AccountEvent.DeviceCommandIncoming(command = DeviceCommandIncoming.TabReceived(null, mock()))))

        verify(callback).invoke(eq(null), any())
    }

    @Test
    fun `only TabReceived commands are delivered`() {
        val callback: (Device?, List<TabData>) -> Unit = mock()
        val observer = EventsObserver(callback)
        val events = listOf(
            AccountEvent.ProfileUpdated,
            AccountEvent.DeviceCommandIncoming(command = DeviceCommandIncoming.TabReceived(mock(), mock())),
            AccountEvent.DeviceCommandIncoming(command = DeviceCommandIncoming.TabReceived(mock(), mock())),
        )

        observer.onEvents(events)

        verify(callback, times(2)).invoke(any(), any())
    }
}
