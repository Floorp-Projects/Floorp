/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.RemoteTabsStorage
import mozilla.components.browser.storage.sync.SyncClient
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class SyncedTabsFeatureTest {
    private lateinit var store: BrowserStore
    private lateinit var tabsStorage: RemoteTabsStorage
    private lateinit var accountManager: FxaAccountManager

    @Before
    fun setup() {
        store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = "tab1", url = "https://www.mozilla.org"),
                createTab(id = "tab2", url = "https://www.foo.bar"),
                createTab(id = "private", url = "https://private.tab", private = true)
            ),
            selectedTabId = "tab1"
        )))
        tabsStorage = mock()
        accountManager = mock()
    }

    @Test
    fun `listens to browser store changes and stores its state`() = runBlocking {
        val feature = SyncedTabsFeature(accountManager, store, tabsStorage)
        feature.start()
        // This action won't change the state, but will run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
        verify(tabsStorage).store(listOf(
            Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 0),
            Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 0)
            // Private tab is absent.
        ))
    }

    @Test
    fun `stops listening to browser store changes on stop()`() = runBlocking {
        val feature = SyncedTabsFeature(accountManager, store, tabsStorage)
        feature.start()
        // Run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
        verify(tabsStorage).store(listOf(
            Tab(history = listOf(TabEntry(title = "", url = "https://www.mozilla.org", iconUrl = null)), active = 0, lastUsed = 0),
            Tab(history = listOf(TabEntry(title = "", url = "https://www.foo.bar", iconUrl = null)), active = 0, lastUsed = 0)
        ))

        feature.stop()
        // Run the flow.
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
        verify(tabsStorage, never()).store(listOf() /* any() is not working so we send garbage */)
    }

    @Test
    fun `getSyncedTabs matches tabs with FxA devices`() = runBlocking {
        val feature = spy(SyncedTabsFeature(accountManager, store, tabsStorage))
        val device1 = Device(
            id = "client1",
            displayName = "Foo Client",
            deviceType = DeviceType.DESKTOP,
            isCurrentDevice = false,
            lastAccessTime = null,
            capabilities = listOf(),
            subscriptionExpired = false,
            subscription = null
        )
        val device2 = Device(
            id = "client2",
            displayName = "Bar Client",
            deviceType = DeviceType.MOBILE,
            isCurrentDevice = false,
            lastAccessTime = null,
            capabilities = listOf(),
            subscriptionExpired = false,
            subscription = null
        )
        doReturn(listOf(device1, device2)).`when`(feature).syncClients()
        val tabsClient1 = listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0))
        val tabsClient2 = listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0))
        whenever(tabsStorage.getAll()).thenReturn(mapOf(
            SyncClient("client1") to tabsClient1,
            SyncClient("client2") to tabsClient2,
            SyncClient("client-unknown") to listOf(Tab(listOf(TabEntry("Foo", "https://foo.bar", null)), 0, 0))
        ))
        assertEquals(mapOf(
            device1 to tabsClient1,
            device2 to tabsClient2
        ), feature.getSyncedTabs())
    }

    @Test
    fun `getSyncedTabs returns empty list if syncClients() is null`() = runBlocking {
        val feature = spy(SyncedTabsFeature(accountManager, store, tabsStorage))
        doReturn(null).`when`(feature).syncClients()
        assertEquals(emptyMap<Device, List<Tab>>(), feature.getSyncedTabs())
    }

    @Test
    fun `syncClients returns clients if the account is set and constellation state is set too`() {
        val feature = spy(SyncedTabsFeature(accountManager, store, tabsStorage))
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val state: ConstellationState = mock()
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(account.deviceConstellation()).thenReturn(constellation)
        whenever(constellation.state()).thenReturn(state)
        val otherDevices = listOf(Device(
            id = "client2",
            displayName = "Bar Client",
            deviceType = DeviceType.MOBILE,
            isCurrentDevice = false,
            lastAccessTime = null,
            capabilities = listOf(),
            subscriptionExpired = false,
            subscription = null
        ))
        whenever(state.otherDevices).thenReturn(otherDevices)
        assertEquals(otherDevices, feature.syncClients())
    }

    @Test
    fun `syncClients returns null if the account is set but constellation state is null`() {
        val feature = spy(SyncedTabsFeature(accountManager, store, tabsStorage))
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(account.deviceConstellation()).thenReturn(constellation)
        whenever(constellation.state()).thenReturn(null)
        assertEquals(null, feature.syncClients())
    }

    @Test
    fun `syncClients returns null if the account is null`() {
        val feature = spy(SyncedTabsFeature(accountManager, store, tabsStorage))
        whenever(accountManager.authenticatedAccount()).thenReturn(null)
        assertEquals(null, feature.syncClients())
    }
}
