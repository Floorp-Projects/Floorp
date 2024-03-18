/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.ext

import kotlinx.coroutines.test.runTest
import mozilla.components.feature.syncedtabs.helper.getDevice1Tabs
import mozilla.components.feature.syncedtabs.helper.getDevice2Tabs
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.mockito.Mockito.doReturn

class SyncedTabsStorageKtTest {
    private val syncedTabs: SyncedTabsStorage = mock()

    @Test
    fun `GIVEN synced tabs exist WHEN asked for active device tabs THEN return all tabs`() = runTest {
        val device1Tabs = getDevice1Tabs()
        val device2Tabs = getDevice2Tabs()
        doReturn(listOf(device1Tabs, device2Tabs)).`when`(syncedTabs).getSyncedDeviceTabs()

        val result = syncedTabs.getActiveDeviceTabs()
        assertNotNull(result)
        assertEquals(4, result.size)
        assertEquals(3, result.filter { it.clientName == device1Tabs.device.displayName }.size)
        assertEquals(1, result.filter { it.clientName == device2Tabs.device.displayName }.size)
    }

    @Test
    fun `GIVEN synced tabs exist WHEN asked for a lower number of active device tabs THEN return tabs up to that number`() = runTest {
        val device1Tabs = getDevice1Tabs()
        val device2Tabs = getDevice2Tabs()
        doReturn(listOf(device1Tabs, device2Tabs)).`when`(syncedTabs).getSyncedDeviceTabs()

        var result = syncedTabs.getActiveDeviceTabs(2)
        assertNotNull(result)
        assertEquals(2, result.size)
        assertEquals(2, result.filter { it.clientName == device1Tabs.device.displayName }.size)

        result = syncedTabs.getActiveDeviceTabs(7)
        assertNotNull(result)
        assertEquals(4, result.size)
        assertEquals(3, result.filter { it.clientName == device1Tabs.device.displayName }.size)
        assertEquals(1, result.filter { it.clientName == device2Tabs.device.displayName }.size)
    }

    @Test
    fun `GIVEN synced tabs exist WHEN asked for active device tabs and a filter is passed THEN return all tabs matching the filter`() = runTest {
        val device1Tabs = getDevice1Tabs()
        val device2Tabs = getDevice2Tabs()
        doReturn(listOf(device1Tabs, device2Tabs)).`when`(syncedTabs).getSyncedDeviceTabs()
        val filteredTitle = device1Tabs.tabs[0].active().title

        val result = syncedTabs.getActiveDeviceTabs {
            it.title == filteredTitle
        }
        assertNotNull(result)
        assertEquals(1, result.size)
        assertEquals(filteredTitle, result[0].tab.title)
    }
}
