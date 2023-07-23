/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.DeviceType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.tabstray.ext.toComposeList
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem

class SyncedDeviceTabsTest {
    private val noTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Charcoal"
            every { id } returns "123"
            every { deviceType } returns DeviceType.DESKTOP
        },
        tabs = emptyList(),
    )

    private val oneTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Charcoal"
            every { id } returns "1234"
            every { deviceType } returns DeviceType.DESKTOP
        },
        tabs = listOf(
            Tab(
                history = listOf(
                    TabEntry(
                        title = "Mozilla",
                        url = "https://mozilla.org",
                        iconUrl = null,
                    ),
                ),
                active = 0,
                lastUsed = 0L,
            ),
        ),
    )

    private val twoTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Emerald"
            every { id } returns "12345"
            every { deviceType } returns DeviceType.MOBILE
        },
        tabs = listOf(
            Tab(
                history = listOf(
                    TabEntry(
                        title = "Mozilla",
                        url = "https://mozilla.org",
                        iconUrl = null,
                    ),
                ),
                active = 0,
                lastUsed = 0L,
            ),
            Tab(
                history = listOf(
                    TabEntry(
                        title = "Firefox",
                        url = "https://firefox.com",
                        iconUrl = null,
                    ),
                ),
                active = 0,
                lastUsed = 0L,
            ),
        ),
    )

    @Test
    fun `GIVEN two synced devices WHEN the compose list is generated THEN two device section is returned`() {
        val syncedDeviceList = listOf(oneTabDevice, twoTabDevice)
        val listData = syncedDeviceList.toComposeList()

        assertEquals(2, listData.count())
        assertTrue(listData[0] is SyncedTabsListItem.DeviceSection)
        assertEquals(oneTabDevice.tabs.size, (listData[0] as SyncedTabsListItem.DeviceSection).tabs.size)
        assertTrue(listData[1] is SyncedTabsListItem.DeviceSection)
        assertEquals(twoTabDevice.tabs.size, (listData[1] as SyncedTabsListItem.DeviceSection).tabs.size)
    }

    @Test
    fun `GIVEN one synced device with no tabs WHEN the compose list is generated THEN one device with an empty tabs list is returned`() {
        val syncedDeviceList = listOf(noTabDevice)
        val listData = syncedDeviceList.toComposeList()

        assertEquals(1, listData.count())
        assertTrue(listData[0] is SyncedTabsListItem.DeviceSection)
        assertEquals(0, (listData[0] as SyncedTabsListItem.DeviceSection).tabs.size)
    }
}
