/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

class RecentSyncedTabFeatureTest {
    private val earliestTime = 100L
    private val earlierTime = 250L
    private val timeNow = 500L
    private val currentDevice = Device(
        id = "currentId",
        displayName = "currentDevice",
        deviceType = DeviceType.MOBILE,
        isCurrentDevice = true,
        lastAccessTime = timeNow,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null
    )
    private val deviceAccessed1 = Device(
        id = "id1",
        displayName = "device1",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = earliestTime,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null
    )
    private val deviceAccessed2 = Device(
        id = "id2",
        displayName = "device2",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = earlierTime,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null
    )

    private val store: AppStore = mockk()
    private val accountManager: FxaAccountManager = mockk()

    private lateinit var feature: RecentSyncedTabFeature

    @Before
    fun setup() {
        every { store.dispatch(any()) } returns mockk()

        feature = RecentSyncedTabFeature(
            store = store,
            accountManager = accountManager,
            context = mockk(),
            storage = mockk(),
            lifecycleOwner = mockk(),
        )
    }

    @Test
    fun `WHEN loading is started THEN loading state is dispatched`() {
        feature.startLoading()

        verify { store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Loading)) }
    }

    @Test
    fun `WHEN empty synced tabs are displayed THEN no action is dispatched`() {
        feature.displaySyncedTabs(listOf())

        verify(exactly = 0) { store.dispatch(any()) }
    }

    @Test
    fun `WHEN displaying synced tabs THEN first active tab is used`() {
        val tab = createActiveTab("title", "https://mozilla.org", null)
        val displayedTabs = listOf(SyncedDeviceTabs(deviceAccessed1, listOf(tab)))

        feature.displaySyncedTabs(displayedTabs)

        val expectedTab = tab.toRecentSyncedTab(deviceAccessed1)

        verify {
            store.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `WHEN displaying synced tabs THEN current device is filtered out`() {
        val localTab = createActiveTab("local", "https://local.com", null)
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val displayedTabs = listOf(
            SyncedDeviceTabs(currentDevice, listOf(localTab)),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab))
        )

        feature.displaySyncedTabs(displayedTabs)

        val expectedTab = remoteTab.toRecentSyncedTab(deviceAccessed1)

        verify {
            store.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `WHEN displaying synced tabs THEN any devices with empty tabs list are filtered out`() {
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val displayedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed2, listOf()),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab))
        )

        feature.displaySyncedTabs(displayedTabs)

        val expectedTab = remoteTab.toRecentSyncedTab(deviceAccessed1)

        verify {
            store.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `WHEN displaying synced tabs THEN most recently accessed device is used`() {
        val firstTab = createActiveTab("first", "https://local.com", null)
        val secondTab = createActiveTab("remote", "https://mozilla.org", null)
        val displayedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed1, listOf(firstTab)),
            SyncedDeviceTabs(deviceAccessed2, listOf(secondTab))
        )

        feature.displaySyncedTabs(displayedTabs)

        val expectedTab = secondTab.toRecentSyncedTab(deviceAccessed2)

        verify {
            store.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `WHEN error is received THEN action dispatched with empty synced state`() {
        feature.onError(SyncedTabsView.ErrorType.NO_TABS_AVAILABLE)

        verify { store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    private fun createActiveTab(
        title: String = "title",
        url: String = "url",
        iconUrl: String? = null,
    ): Tab {
        val tab = mockk<Tab>()
        val tabEntry = TabEntry(title, url, iconUrl)
        every { tab.active() } returns tabEntry
        return tab
    }

    private fun Tab.toRecentSyncedTab(device: Device) = RecentSyncedTab(
        deviceDisplayName = device.displayName,
        title = this.active().title,
        url = this.active().url,
        iconUrl = this.active().iconUrl
    )
}
