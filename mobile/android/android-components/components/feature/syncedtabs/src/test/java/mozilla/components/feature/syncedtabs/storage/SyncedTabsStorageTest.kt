/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.storage

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.browser.storage.sync.SyncClient
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class SyncedTabsStorageTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore
    private lateinit var tabsStorage: RemoteTabsStorage
    private lateinit var accountManager: FxaAccountManager

    @Before
    fun setup() {
        store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(id = "tab1", url = "https://www.mozilla.org", lastAccess = 123L),
                        createTab(id = "tab2", url = "https://www.foo.bar", lastAccess = 124L),
                        createTab(id = "private", url = "https://private.tab", private = true, lastAccess = 125L),
                    ),
                    selectedTabId = "tab1",
                ),
            ),
        )
        tabsStorage = mock()
        accountManager = mock()
    }

    @Test
    fun `listens to browser store changes, stores state changes, and calls onStoreComplete`() = runTestOnMain {
        val feature = SyncedTabsStorage(
            accountManager,
            store,
            tabsStorage,
            debounceMillis = 0,
        )
        feature.start()

        // This action will change the state due to lastUsed timestamp, but will run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        verify(tabsStorage, times(2)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
                // Private tab is absent.
            ),
        )
        verify(accountManager, times(2)).syncNow(
            SyncReason.User,
            true,
            listOf(SyncEngine.Tabs),
        )
    }

    @Test
    fun `stops listening to browser store changes on stop()`() = runTestOnMain {
        val feature = SyncedTabsStorage(
            accountManager,
            store,
            tabsStorage,
            debounceMillis = 0,
        )
        feature.start()
        // Run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        verify(tabsStorage, times(2)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
            ),
        )

        feature.stop()
        // Run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        verify(tabsStorage, never()).store(listOf() /* any() is not working so we send garbage */)
    }

    @Test
    fun `getSyncedTabs matches tabs with FxA devices`() = runTestOnMain {
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
            ),
        )
        val device1 = Device(
            id = "client1",
            displayName = "Foo Client",
            deviceType = DeviceType.DESKTOP,
            isCurrentDevice = false,
            lastAccessTime = null,
            capabilities = listOf(),
            subscriptionExpired = false,
            subscription = null,
        )
        val device2 = Device(
            id = "client2",
            displayName = "Bar Client",
            deviceType = DeviceType.MOBILE,
            isCurrentDevice = false,
            lastAccessTime = null,
            capabilities = listOf(),
            subscriptionExpired = false,
            subscription = null,
        )
        doReturn(listOf(device1, device2)).`when`(feature).syncClients()
        val tabsClient1 = listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0))
        val tabsClient2 = listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0))
        whenever(tabsStorage.getAll()).thenReturn(
            mapOf(
                SyncClient("client1") to tabsClient1,
                SyncClient("client2") to tabsClient2,
                SyncClient("client-unknown") to listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0)),
            ),
        )

        val result = feature.getSyncedDeviceTabs()
        assertEquals(device1, result[0].device)
        assertEquals(device2, result[1].device)
        assertEquals(tabsClient1, result[0].tabs)
        assertEquals(tabsClient2, result[1].tabs)
    }

    @Test
    fun `getSyncedTabs returns empty list if syncClients() is null`() = runTestOnMain {
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
            ),
        )
        doReturn(null).`when`(feature).syncClients()
        assertEquals(emptyList<SyncedDeviceTabs>(), feature.getSyncedDeviceTabs())
    }

    @Test
    fun `syncClients returns clients if the account is set and constellation state is set too`() {
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
            ),
        )
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val state: ConstellationState = mock()
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(account.deviceConstellation()).thenReturn(constellation)
        whenever(constellation.state()).thenReturn(state)
        val otherDevices = listOf(
            Device(
                id = "client2",
                displayName = "Bar Client",
                deviceType = DeviceType.MOBILE,
                isCurrentDevice = false,
                lastAccessTime = null,
                capabilities = listOf(),
                subscriptionExpired = false,
                subscription = null,
            ),
        )
        whenever(state.otherDevices).thenReturn(otherDevices)
        assertEquals(otherDevices, feature.syncClients())
    }

    @Test
    fun `syncClients returns null if the account is set but constellation state is null`() {
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
            ),
        )
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(account.deviceConstellation()).thenReturn(constellation)
        whenever(constellation.state()).thenReturn(null)
        assertEquals(null, feature.syncClients())
    }

    @Test
    fun `syncClients returns null if the account is null`() {
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
            ),
        )
        whenever(accountManager.authenticatedAccount()).thenReturn(null)
        assertEquals(null, feature.syncClients())
    }

    @Test
    fun `tabs are stored when loaded`() = runTestOnMain {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://www.mozilla.org", lastAccess = 123L),
                    createTab(id = "tab2", url = "https://www.foo.bar", lastAccess = 124L),
                ),
                selectedTabId = "tab1",
            ),
        )
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
                debounceMillis = 0,
            ),
        )
        feature.start()

        // Tabs are only stored when initial state is collected, since they are already loaded
        verify(tabsStorage, times(1)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
            ),
        )

        // Change a tab besides loading it
        store.dispatch(ContentAction.UpdateProgressAction("tab1", 50)).joinBlocking()

        reset(tabsStorage)

        verify(tabsStorage, never()).store(any())
    }

    @Test
    fun `only loaded tabs are stored on load`() = runTestOnMain {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createUnloadedTab(id = "tab1", url = "https://www.mozilla.org", lastAccess = 123L),
                    createUnloadedTab(id = "tab2", url = "https://www.foo.bar", lastAccess = 124L),
                ),
                selectedTabId = "tab1",
            ),
        )
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
                debounceMillis = 0,
            ),
        )
        feature.start()

        store.dispatch(ContentAction.UpdateLoadingStateAction("tab1", false)).joinBlocking()

        verify(tabsStorage).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
            ),
        )
    }

    @Test
    fun `tabs are stored when selected tab changes`() = runTestOnMain {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://www.mozilla.org", lastAccess = 123L),
                    createTab(id = "tab2", url = "https://www.foo.bar", lastAccess = 124L),
                ),
                selectedTabId = "tab1",
            ),
        )
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
                debounceMillis = 0,
            ),
        )
        feature.start()

        store.dispatch(TabListAction.SelectTabAction("tab2")).joinBlocking()

        verify(tabsStorage, times(2)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
            ),
        )
    }

    @Test
    fun `tabs are stored when lastAccessed is changed for any tab`() = runTestOnMain {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://www.mozilla.org", lastAccess = 123L),
                    createTab(id = "tab2", url = "https://www.foo.bar", lastAccess = 124L),
                ),
                selectedTabId = "tab1",
            ),
        )
        val feature = spy(
            SyncedTabsStorage(
                accountManager,
                store,
                tabsStorage,
                debounceMillis = 0,
            ),
        )
        feature.start()

        store.dispatch(LastAccessAction.UpdateLastAccessAction("tab1", 300L)).joinBlocking()

        verify(tabsStorage, times(1)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 123L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
            ),
        )
        verify(tabsStorage, times(1)).store(
            listOf(
                Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 300L),
                Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 124L),
            ),
        )
    }

    private fun createUnloadedTab(id: String, url: String, lastAccess: Long) = createTab(id = id, url = url, lastAccess = lastAccess).run {
        copy(content = this.content.copy(loading = true))
    }
}
