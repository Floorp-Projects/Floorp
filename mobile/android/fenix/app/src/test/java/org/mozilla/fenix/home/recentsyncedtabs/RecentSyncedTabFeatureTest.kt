/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkConstructor
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
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.store.Account
import mozilla.components.service.fxa.store.SyncAction
import mozilla.components.service.fxa.store.SyncStatus
import mozilla.components.service.fxa.store.SyncStore
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.internal.ErrorType
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
        subscription = null,
    )
    private val deviceAccessed1 = Device(
        id = "id1",
        displayName = "device1",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = earliestTime,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null,
    )
    private val deviceAccessed2 = Device(
        id = "id2",
        displayName = "device2",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = earlierTime,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null,
    )

    private val appStore: AppStore = mockk()
    private val accountManager: FxaAccountManager = mockk(relaxed = true)
    private val syncedTabsStorage: SyncedTabsStorage = mockk()
    private val historyStorage: HistoryStorage = mockk()

    private val syncStore = SyncStore()

    private lateinit var feature: RecentSyncedTabFeature

    @Before
    fun setup() {
        Dispatchers.setMain(StandardTestDispatcher())

        every { appStore.dispatch(any()) } returns mockk()
        mockkConstructor(SyncEnginesStorage::class)
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns mapOf(
            SyncEngine.Tabs to true,
        )

        feature = RecentSyncedTabFeature(
            context = testContext,
            appStore = appStore,
            syncStore = syncStore,
            accountManager = accountManager,
            storage = syncedTabsStorage,
            historyStorage = historyStorage,
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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val activeTab = createActiveTab()
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(
                device = deviceAccessed1,
                tabs = listOf(activeTab),
            ),
        )

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expected = listOf(activeTab.toRecentSyncedTab(deviceAccessed1))
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
    }

    @Test
    fun `GIVEN loading state has not been dispatched WHEN status becomes idle THEN timing distribution is not recorded`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val activeTab = createActiveTab()
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(
                device = deviceAccessed1,
                tabs = listOf(activeTab),
            ),
        )

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()
        // this does not trigger a loading state, which should only be shown when tabs are loaded
        // during app initialization
        syncStore.setState(status = SyncStatus.Started)
        runCurrent()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        assertEquals(
            0,
            RecentSyncedTabs.recentSyncedTabTimeToLoad.testGetNumRecordedErrors(
                ErrorType.INVALID_STATE,
            ),
        )
    }

    @Test
    fun `GIVEN tabs from remote and current devices WHEN dispatching recent synced tab THEN current device is filtered out of dispatch`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val localTab = createActiveTab("local", "https://local.com", null)
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(currentDevice, listOf(localTab)),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab)),
        )
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expectedTabs = listOf(remoteTab.toRecentSyncedTab(deviceAccessed1))
        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTabs)),
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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val remoteTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed2, listOf()),
            SyncedDeviceTabs(deviceAccessed1, listOf(remoteTab)),
        )
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expectedTabs = listOf(remoteTab.toRecentSyncedTab(deviceAccessed1))
        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTabs)),
            )
        }
    }

    @Test
    fun `GIVEN tabs from different remote devices WHEN dispatching recent synced tab THEN most recently accessed tabs are set in the Success state`() =
        runTest {
            val account = mockk<Account>()
            syncStore.setState(account = account)
            every { appStore.state } returns mockk {
                every { recentSyncedTabState } returns RecentSyncedTabState.Loading
            }
            coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
            val firstDeviceTabs = listOf(
                createActiveTab("first", "https://local.com", null),
                createActiveTab("second", "https://github.com", null),
            )
            val secondDeviceTabs = listOf(
                createActiveTab("first", "https://mozilla.org", null),
                createActiveTab("second", "https://www.mozilla.org/en-US/firefox", null),
            )
            val currentTime = System.currentTimeMillis()
            // Delay used to change last used times of tabs
            val usedDelay = 5 * 60 * 1000
            every { firstDeviceTabs[0].lastUsed } returns currentTime
            every { firstDeviceTabs[1].lastUsed } returns currentTime - 2 * usedDelay
            every { secondDeviceTabs[0].lastUsed } returns currentTime - usedDelay
            every { secondDeviceTabs[1].lastUsed } returns currentTime - 3 * usedDelay
            val syncedTabs = listOf(
                SyncedDeviceTabs(deviceAccessed1, firstDeviceTabs),
                SyncedDeviceTabs(deviceAccessed2, secondDeviceTabs),
            )
            coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns syncedTabs

            feature.start()
            syncStore.setState(status = SyncStatus.Idle)
            runCurrent()

            // The order of the tabs should be given by the `lastUsed` property
            val expectedTabs =
                (firstDeviceTabs + secondDeviceTabs).sortedByDescending { it.lastUsed }.map {
                    if (it in firstDeviceTabs) {
                        it.toRecentSyncedTab(deviceAccessed1)
                    } else {
                        it.toRecentSyncedTab(deviceAccessed2)
                    }
                }
            verify {
                appStore.dispatch(
                    AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expectedTabs)),
                )
            }
        }

    @Test
    fun `GIVEN sync tabs are disabled WHEN dispatching recent synced tab THEN dispatch none`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns mapOf(
            SyncEngine.Tabs to false,
        )

        val firstTab = createActiveTab("remote", "https://mozilla.org", null)
        val syncedTabs = listOf(
            SyncedDeviceTabs(deviceAccessed1, listOf(firstTab)),
        )
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns syncedTabs

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        verify {
            appStore.dispatch(
                AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None),
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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(tab)

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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(tab)

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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val tab = SyncedDeviceTabs(deviceAccessed1, listOf(createActiveTab()))
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(tab)

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
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returnsMany listOf(tabs1, tabs2)

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
                RecentSyncedTabState.Loading,
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
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf()
        val tab = createActiveTab()
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(deviceAccessed1, listOf(tab)),
        )

        feature.start()
        runCurrent()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()
        syncStore.setState(status = SyncStatus.LoggedOut)
        runCurrent()

        val expected = listOf(tab.toRecentSyncedTab(deviceAccessed1))
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.None)) }
    }

    @Test
    fun `GIVEN history entry contains synced tab host and has a preview image URL WHEN dispatched THEN preview url is included`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val activeTab = createActiveTab()
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(
                device = deviceAccessed1,
                tabs = listOf(activeTab),
            ),
        )
        val longerThanSyncedTabUrl = activeTab.active().url + "/some/more/paths"
        val previewUrl = "preview"
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf(
            activeTab.toVisitInfo(longerThanSyncedTabUrl, previewUrl),
        )

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expected = listOf(activeTab.toRecentSyncedTab(deviceAccessed1, previewUrl))
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
    }

    @Test
    fun `GIVEN history entry contains synced tab host but has no preview image URL WHEN dispatched THEN preview url is not included`() = runTest {
        val account = mockk<Account>()
        syncStore.setState(account = account)
        every { appStore.state } returns mockk {
            every { recentSyncedTabState } returns RecentSyncedTabState.Loading
        }
        val activeTab = createActiveTab()
        coEvery { syncedTabsStorage.getSyncedDeviceTabs() } returns listOf(
            SyncedDeviceTabs(
                device = deviceAccessed1,
                tabs = listOf(activeTab),
            ),
        )
        val longerThanSyncedTabUrl = activeTab.active().url + "/some/more/paths"
        coEvery { historyStorage.getDetailedVisits(any(), any()) } returns listOf(
            activeTab.toVisitInfo(longerThanSyncedTabUrl, null),
        )

        feature.start()
        syncStore.setState(status = SyncStatus.Idle)
        runCurrent()

        val expected = listOf(activeTab.toRecentSyncedTab(deviceAccessed1, null))
        verify { appStore.dispatch(AppAction.RecentSyncedTabStateChange(RecentSyncedTabState.Success(expected))) }
    }

    private fun createActiveTab(
        title: String = "title",
        url: String = "url",
        iconUrl: String? = null,
    ): Tab {
        val tab = mockk<Tab>()
        val tabEntry = TabEntry(title, url, iconUrl)
        every { tab.active() } returns tabEntry
        every { tab.lastUsed } returns System.currentTimeMillis()
        return tab
    }

    private fun Tab.toRecentSyncedTab(
        device: Device,
        previewImageUrl: String? = null,
    ) = RecentSyncedTab(
        deviceDisplayName = device.displayName,
        deviceType = device.deviceType,
        title = this.active().title,
        url = this.active().url,
        previewImageUrl = previewImageUrl,
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

    private fun Tab.toVisitInfo(url: String, previewUrl: String?) = VisitInfo(
        title = this.active().title,
        url = url,
        visitTime = 0L,
        visitType = VisitType.TYPED,
        previewImageUrl = previewUrl,
        isRemote = false,
    )
}
