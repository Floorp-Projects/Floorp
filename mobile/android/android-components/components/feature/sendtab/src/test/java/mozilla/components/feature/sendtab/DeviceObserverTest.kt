/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.sendtab

import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.TabData
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class DeviceObserverTest {
    @Test
    fun `events are delivered successfully`() {
        val callback: (Device?, List<TabData>) -> Unit = mock()
        val observer = DeviceObserver(callback)
        val events = listOf(DeviceEvent.TabReceived(mock(), mock()))

        observer.onEvents(events)

        verify(callback).invoke(any(), any())

        observer.onEvents(listOf(DeviceEvent.TabReceived(null, mock())))

        verify(callback).invoke(eq(null), any())
    }

    @Test
    fun `only TabReceived events are delivered`() {
        // we don't have other event types right now so this is a basic test.
        val callback: (Device?, List<TabData>) -> Unit = mock()
        val observer = DeviceObserver(callback)
        val events = listOf(
            DeviceEvent.TabReceived(mock(), mock()),
            DeviceEvent.TabReceived(mock(), mock())
        )

        observer.onEvents(events)

        verify(callback, times(2)).invoke(any(), any())
    }
}