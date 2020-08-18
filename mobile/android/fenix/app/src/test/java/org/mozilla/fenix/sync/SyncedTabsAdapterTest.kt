/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.widget.FrameLayout
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SyncedTabsAdapterTest {

    private lateinit var listener: SyncedTabsView.Listener
    private lateinit var adapter: SyncedTabsAdapter

    private val oneTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Charcoal"
            every { deviceType } returns DeviceType.DESKTOP
        },
        tabs = listOf(Tab(
            history = listOf(TabEntry(
                title = "Mozilla",
                url = "https://mozilla.org",
                iconUrl = null
            )),
            active = 0,
            lastUsed = 0L
        ))
    )

    private val threeTabDevice = SyncedDeviceTabs(
        device = mockk {
            every { displayName } returns "Emerald"
            every { deviceType } returns DeviceType.MOBILE
        },
        tabs = listOf(
            Tab(
                history = listOf(TabEntry(
                    title = "Mozilla",
                    url = "https://mozilla.org",
                    iconUrl = null
                )),
                active = 0,
                lastUsed = 0L
            ),
            Tab(
                history = listOf(TabEntry(
                    title = "Firefox",
                    url = "https://firefox.com",
                    iconUrl = null
                )),
                active = 0,
                lastUsed = 0L
            )
        )
    )

    @Before
    fun setup() {
        listener = mockk(relaxed = true)
        adapter = SyncedTabsAdapter(listener)
    }

    @Test
    fun `updateData() adds items for each device and tab`() {
        assertEquals(0, adapter.itemCount)

        adapter.updateData(
            listOf(
                oneTabDevice,
                threeTabDevice
            )
        )

        assertEquals(5, adapter.itemCount)
        assertEquals(SyncedTabsViewHolder.DeviceViewHolder.LAYOUT_ID, adapter.getItemViewType(0))
        assertEquals(SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID, adapter.getItemViewType(1))
        assertEquals(SyncedTabsViewHolder.DeviceViewHolder.LAYOUT_ID, adapter.getItemViewType(2))
        assertEquals(SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID, adapter.getItemViewType(3))
        assertEquals(SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID, adapter.getItemViewType(4))
    }

    @Test
    fun `adapter can create and bind viewholders for SyncedDeviceTabs`() {
        val parent = FrameLayout(testContext)
        adapter.updateData(listOf(oneTabDevice))

        val deviceHolder = adapter.createViewHolder(parent, SyncedTabsViewHolder.DeviceViewHolder.LAYOUT_ID)
        val tabHolder = adapter.createViewHolder(parent, SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID)

        // Should not throw
        adapter.bindViewHolder(deviceHolder, 0)
        adapter.bindViewHolder(tabHolder, 1)
    }
}
