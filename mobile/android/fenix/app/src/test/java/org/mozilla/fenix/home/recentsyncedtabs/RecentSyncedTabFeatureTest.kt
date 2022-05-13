/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
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
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.RecentSyncedTabs
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

@RunWith(AndroidJUnit4::class)
class RecentSyncedTabFeatureTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

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
    fun `GIVEN that there is no current state WHEN loading is started THEN loading state is dispatched`() {
        every { store.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }

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
    fun `WHEN synced tab displayed THEN labeled counter metric recorded with device type`() {
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))

        feature.displaySyncedTabs(listOf(tab))

        assertEquals(1, RecentSyncedTabs.recentSyncedTabShown["desktop"].testGetValue())
    }

    @Test
    fun `GIVEN that tab previously started loading WHEN synced tab displayed THEN load time metric recorded`() {
        every { store.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))

        feature.startLoading()
        feature.displaySyncedTabs(listOf(tab))

        assertNotNull(RecentSyncedTabs.recentSyncedTabTimeToLoad.testGetValue())
    }

    @Test
    fun `GIVEN that the displayed tab was the last displayed tab WHEN displayed THEN recorded as stale`() {
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))

        feature.displaySyncedTabs(listOf(tab))
        feature.displaySyncedTabs(listOf(tab))

        assertEquals(1, RecentSyncedTabs.latestSyncedTabIsStale.testGetValue())
    }

    @Test
    fun `GIVEN that the displayed tab was not the last displayed tab WHEN displayed THEN not recorded as stale`() {
        val tab1 = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        val tab2 = SyncedDeviceTabs(deviceAccessed2, listOf(createActiveTab()))

        feature.displaySyncedTabs(listOf(tab1))
        feature.displaySyncedTabs(listOf(tab2))

        assertNull(RecentSyncedTabs.latestSyncedTabIsStale.testGetValue())
    }

    @Test
    fun `GIVEN that no tab is displayed WHEN stopLoading is called THEN none state dispatched`() {
        feature.stopLoading()

        verify { store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN that a tab is displayed WHEN stopLoading is called THEN nothing dispatched`() {
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))

        feature.displaySyncedTabs(listOf(tab))
        feature.stopLoading()

        verify(exactly = 0) { store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN that feature is not loading WHEN error received THEN does not dispatch NONE state`() {
        every { store.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        feature.onError(SyncedTabsView.ErrorType.NO_TABS_AVAILABLE)

        verify(exactly = 0) { store.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN that feature is loading WHEN error received THEN dispatches NONE state`() {
        every { store.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }

        feature.onError(SyncedTabsView.ErrorType.MULTIPLE_DEVICES_UNAVAILABLE)

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
        deviceType = device.deviceType,
        title = this.active().title,
        url = this.active().url,
        iconUrl = this.active().iconUrl
    )
}
