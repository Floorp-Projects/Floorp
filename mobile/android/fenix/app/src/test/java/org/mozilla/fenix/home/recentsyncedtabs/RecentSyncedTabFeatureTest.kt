/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.store.Account
import mozilla.components.service.fxa.store.SyncAction
import mozilla.components.service.fxa.store.SyncStatus
import mozilla.components.service.fxa.store.SyncStore
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
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

    private val appStore: AppStore = mockk()
    private val accountManager: FxaAccountManager = mockk(relaxed = true)
    private val storage: SyncedTabsStorage = mockk()

    private val syncStore = SyncStore()

    private lateinit var feature: RecentSyncedTabFeature

    @Before
    fun setup() {
        Dispatchers.setMain(StandardTestDispatcher())

        every { appStore.dispatch(any()) } returns mockk()

        feature = RecentSyncedTabFeature(
            appStore = appStore,
            syncStore = syncStore,
            accountManager = accountManager,
            storage = storage,
            coroutineScope = TestScope(),
        )
    }

    @Test
    fun `GIVEN account is not available WHEN started THEN nothing is dispatched`() {
        feature.start()

        verify(exactly = 0) { appStore.dispatch(any()) }
    }

    @Test
    fun `GIVEN current tab state is none WHEN account becomes available THEN loading state is dispatched, devices are refreshed, and a sync is started`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)

        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }

        feature.start()
        runCurrent()

        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Loading)) }
        coVerify { accountManager.withConstellation { refreshDevices() } }
        coVerify { accountManager.syncNow(reason = SyncReason.User, debounce = true, customEngineSubset = listOf(SyncEngine.Tabs)) }
    }

    @Test
    fun `GIVEN current tab state is not none WHEN account becomes available THEN loading state is not dispatched`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)

        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }

        feature.start()
        runCurrent()

        verify(exactly = 0) { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Loading)) }
    }

    @Test
    fun `GIVEN synced tabs WHEN status becomes idle THEN recent synced tab is dispatched`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val activeTab = createActiveTab()
        coEvery { storage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(
                device = deviceAccessed1,
                tabs = listOf(activeTab)
            )
        )

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expected = activeTab.toRecentSyncedTab(deviceAccessed1)
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
    }

    @Test
    fun `GIVEN tabs from remote and current devices WHEN dispatching recent synced tab THEN current device is filtered out of dispatch`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val localTab = createActiveTab("local", "https://local.com", null)
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(currentDevice, listOf(localTab)),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab))
        )
        coEvery { storage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expectedTab = remoteTab.toRecentSyncedTab(deviceAccessed1)
        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `GIVEN there are devices with empty tabs list WHEN dispatching recent synced tab THEN devices with empty tabs list are filtered out`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed2, listOf()),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab))
        )
        coEvery { storage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expectedTab = remoteTab.toRecentSyncedTab(deviceAccessed1)
        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `GIVEN tabs from different remote devices WHEN dispatching recent synced tab THEN most recently accessed device is used`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val firstTab = createActiveTab("first", "https://local.com", null)
        val secondTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed1, listOf(firstTab)),
            SyncedDeviceTabs(deviceAccessed2, listOf(secondTab))
        )
        coEvery { storage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expectedTab = secondTab.toRecentSyncedTab(deviceAccessed2)
        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTab))
            )
        }
    }

    @Test
    fun `WHEN synced tab dispatched THEN labeled counter metric recorded with device type`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { storage.getSyncedDeviceTabs() } returns listOf(tab)

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        assertEquals(1, RecentSyncedTabs.recentSyncedTabShown["desktop"].testGetValue())
    }

    @Test
    fun `WHEN synced tab dispatched THEN load time metric recorded`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { storage.getSyncedDeviceTabs() } returns listOf(tab)

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        assertNotNull(RecentSyncedTabs.recentSyncedTabTimeToLoad.testGetValue())
    }

    @Test
    fun `GIVEN that the dispatched tab was the last dispatched tab WHEN dispatched THEN recorded as stale`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { storage.getSyncedDeviceTabs() } returns listOf(tab)

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()
        syncStore.setState(status = SyncStatus.Started)
        runCurrent()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        assertEquals(1, RecentSyncedTabs.latestSyncedTabIsStale.testGetValue())
    }

    @Test
    fun `GIVEN that the dispatched tab was not the last dispatched tab WHEN dispatched THEN not recorded as stale`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        val tabs1 = listOf(SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab())))
        val tabs2 = listOf(SyncedDeviceTabs(deviceAccessed2, listOf(createActiveTab())))
        coEvery { storage.getSyncedDeviceTabs() } returnsMany listOf(tabs1, tabs2)

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()
        syncStore.setState(status = SyncStatus.Started)
        runCurrent()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        assertNull(RecentSyncedTabs.latestSyncedTabIsStale.testGetValue())
    }

    @Test
    fun `GIVEN current tab state is loading WHEN error is observed THEN tab state is dispatched as none`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returnsMany listOf(
                RecentSyncedTabState.None,
                RecentSyncedTabState.Loading
            )
        }

        feature.start()
        runCurrent()
        syncStore.setState(status = SyncStatus.Error)
        runCurrent()

        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN current tab state is not loading WHEN error is observed THEN nothing is dispatched`() = runTest {
        feature.start()
        syncStore.setState(status = SyncStatus.Error)
        runCurrent()

        verify(exactly = 0) { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN that a tab has been dispatched WHEN LoggedOut is observed THEN tab state is dispatched as none`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.None
        }
        val tab = createActiveTab()
        coEvery { storage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(deviceAccessed1, listOf(tab))
        )

        feature.start()
        runCurrent()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()
        syncStore.setState(status = SyncStatus.LoggedOut)
        runCurrent()

        val expected = tab.toRecentSyncedTab(deviceAccessed1)
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
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

    private fun SyncStore.setState(
        status: SyncStatus? = null,
        account: Account? = null,
    ) {
        status?.let {
            this.dispatch(SyncAction.UpdateSyncStatus(status))
        }
        account?.let {
            this.dispatch(SyncAction.UpdateAccount(account))
        }
        this.waitUntilIdle()
    }
}
