/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.tabstray.ext.toComposeList
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListSupportedFeature

class SyncedDeviceTabsTest {
    private val noTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Charcoal"
            every { id } returns "123"
            every { deviceType } returns DeviceType.DESKTOP
            every { capabilities } returns emptyList()
        },
        tabs = emptyList(),
    )

    private val oneTabDeviceWithoutCapabilities = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Charcoal"
            every { id } returns "1234"
            every { deviceType } returns DeviceType.DESKTOP
            every { capabilities } returns emptyList()
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
                inactive = false,
            ),
        ),
    )

    private val oneTabDeviceWithCapabilities = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Sapphire"
            every { id } returns "123456"
            every { deviceType } returns DeviceType.MOBILE
            every { capabilities } returns listOf(DeviceCapability.CLOSE_TABS)
        },
        tabs = listOf(
            Tab(
                history = listOf(
                    TabEntry(
                        title = "Thunderbird",
                        url = "https://getthunderbird.org",
                        iconUrl = null,
                    ),
                ),
                active = 0,
                lastUsed = 0L,
                inactive = false,
            ),
        ),
    )

    private val twoTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Emerald"
            every { id } returns "12345"
            every { deviceType } returns DeviceType.MOBILE
            every { capabilities } returns emptyList()
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
                inactive = false,
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
                inactive = false,
            ),
        ),
    )

    @Test
    fun `GIVEN two synced devices WHEN the compose list is generated THEN two device section is returned`() {
        val syncedDeviceList = listOf(oneTabDeviceWithoutCapabilities, twoTabDevice)
        val listData = syncedDeviceList.toComposeList()

        assertEquals(2, listData.count())
        assertTrue(listData[0] is SyncedTabsListItem.DeviceSection)
        assertEquals(oneTabDeviceWithoutCapabilities.tabs.size, (listData[0] as SyncedTabsListItem.DeviceSection).tabs.size)
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

    @Test
    fun `GIVEN two synced devices AND one device supports closing synced tabs AND closing synced tabs is enabled WHEN the compose list is generated THEN two device sections are returned`() {
        val syncedDeviceList = listOf(oneTabDeviceWithoutCapabilities, oneTabDeviceWithCapabilities)
        val listData = syncedDeviceList.toComposeList(setOf(SyncedTabsListSupportedFeature.CLOSE_TABS))
        val deviceSections = listData.filterIsInstance<SyncedTabsListItem.DeviceSection>()

        assertEquals(2, listData.size)
        assertEquals(listData.size, deviceSections.size)

        assertEquals(setOf(SyncedTabsListItem.Tab.Action.None), deviceSections[0].tabs.map { it.action }.toSet())
        assertEquals(setOf(SyncedTabsListItem.Tab.Action.Close(deviceId = "123456")), deviceSections[1].tabs.map { it.action }.toSet())
    }

    @Test
    fun `GIVEN two synced devices AND one device supports closing synced tabs AND closing synced tabs is disabled WHEN the compose list is generated THEN two device sections are returned`() {
        val syncedDeviceList = listOf(oneTabDeviceWithoutCapabilities, oneTabDeviceWithCapabilities)
        val listData = syncedDeviceList.toComposeList(emptySet())
        val deviceSections = listData.filterIsInstance<SyncedTabsListItem.DeviceSection>()

        assertEquals(2, listData.size)
        assertEquals(listData.size, deviceSections.size)

        assertEquals(setOf(SyncedTabsListItem.Tab.Action.None), deviceSections[0].tabs.map { it.action }.toSet())
        assertEquals(setOf(SyncedTabsListItem.Tab.Action.None), deviceSections[1].tabs.map { it.action }.toSet())
    }
}
