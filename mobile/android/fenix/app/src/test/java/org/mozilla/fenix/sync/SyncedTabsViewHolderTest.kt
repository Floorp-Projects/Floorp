/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.SyncTabsListItemBinding
import org.mozilla.fenix.databinding.ViewSyncedTabsGroupBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SyncedTabsViewHolderTest {

    private lateinit var tabViewHolder: SyncedTabsViewHolder.TabViewHolder
    private lateinit var tabView: View
    private lateinit var deviceViewHolder: SyncedTabsViewHolder.DeviceViewHolder
    private lateinit var deviceView: View
    private lateinit var deviceViewGroupName: TextView
    private lateinit var titleView: View
    private lateinit var titleViewHolder: SyncedTabsViewHolder.TitleViewHolder
    private lateinit var noTabsView: View
    private lateinit var noTabsViewHolder: SyncedTabsViewHolder.NoTabsViewHolder

    private lateinit var syncTabsListItemBinding: SyncTabsListItemBinding

    private val tab = Tab(
        history = listOf(
            mockk(),
            TabEntry(
                title = "Firefox",
                url = "https://mozilla.org/mobile",
                iconUrl = "https://mozilla.org/favicon.ico"
            ),
            mockk()
        ),
        active = 1,
        lastUsed = 0L
    )

    @Before
    fun setup() {
        val inflater = LayoutInflater.from(testContext)

        tabView = inflater.inflate(SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID, null)
        tabViewHolder = SyncedTabsViewHolder.TabViewHolder(tabView)

        syncTabsListItemBinding = SyncTabsListItemBinding.bind(tabView)
        val viewSyncedTabsGroupBinding = ViewSyncedTabsGroupBinding.inflate(inflater)
        deviceView = mockk()

        deviceViewHolder = SyncedTabsViewHolder.DeviceViewHolder(spyk(viewSyncedTabsGroupBinding.root))

        deviceViewGroupName = spyk(viewSyncedTabsGroupBinding.syncedTabsGroupName)

        titleView = inflater.inflate(SyncedTabsViewHolder.TitleViewHolder.LAYOUT_ID, null)
        titleViewHolder = SyncedTabsViewHolder.TitleViewHolder(titleView)

        noTabsView = inflater.inflate(SyncedTabsViewHolder.NoTabsViewHolder.LAYOUT_ID, null)
        noTabsViewHolder = SyncedTabsViewHolder.NoTabsViewHolder(noTabsView)
    }

    @Test
    fun `TabViewHolder binds active tab`() {
        tabViewHolder.bind(SyncedTabsAdapter.AdapterItem.Tab(tab), mockk())

        assertEquals("Firefox", syncTabsListItemBinding.syncedTabItemTitle.text)
        assertEquals("mozilla.org", syncTabsListItemBinding.syncedTabItemUrl.text)
    }

    @Test
    fun `TabViewHolder calls interactor on click`() {
        val interactor = mockk<SyncedTabsView.Listener>(relaxed = true)
        tabViewHolder.bind(SyncedTabsAdapter.AdapterItem.Tab(tab), interactor)

        tabView.performClick()
        verify { interactor.onTabClicked(tab) }
    }

    @Test
    fun `DeviceViewHolder binds desktop device`() {
        val device = mockk<Device> {
            every { displayName } returns "Charcoal"
            every { deviceType } returns DeviceType.DESKTOP
        }
        deviceViewHolder.bind(SyncedTabsAdapter.AdapterItem.Device(device), mockk())

        assertEquals("Charcoal", deviceViewHolder.binding.syncedTabsGroupName.text)
    }

    @Test
    fun `DeviceViewHolder binds mobile device`() {
        val device = mockk<Device> {
            every { displayName } returns "Emerald"
            every { deviceType } returns DeviceType.MOBILE
        }
        deviceViewHolder.bind(SyncedTabsAdapter.AdapterItem.Device(device), mockk())

        assertEquals("Emerald", deviceViewHolder.binding.syncedTabsGroupName.text)
    }

    @Test
    fun `TitleViewHolder calls interactor refresh`() {
        val interactor = mockk<SyncedTabsView.Listener>(relaxed = true)
        titleViewHolder.bind(SyncedTabsAdapter.AdapterItem.Title, interactor)

        titleView.findViewById<View>(R.id.refresh_icon).performClick()

        verify { interactor.onRefresh() }
    }

    @Test
    fun `NoTabsViewHolder does nothing`() {
        val device = mockk<Device> {
            every { displayName } returns "Charcoal"
            every { deviceType } returns DeviceType.DESKTOP
        }
        val interactor = mockk<SyncedTabsView.Listener>(relaxed = true)
        noTabsViewHolder.bind(SyncedTabsAdapter.AdapterItem.NoTabs(device), interactor)

        titleView.performClick()

        verify { interactor wasNot Called }
    }
}
