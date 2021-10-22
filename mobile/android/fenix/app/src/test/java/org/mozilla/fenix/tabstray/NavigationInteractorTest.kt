/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.collections.CollectionsDialog
import org.mozilla.fenix.collections.show
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import mozilla.components.browser.state.state.createTab as createStateTab
import mozilla.components.browser.storage.sync.Tab as SyncTab
import org.mozilla.fenix.tabstray.browser.createTab as createTrayTab

class NavigationInteractorTest {
    private lateinit var store: BrowserStore
    private lateinit var tabsTrayStore: TabsTrayStore
    private val testTab: TabSessionState = createStateTab(url = "https://mozilla.org")
    private val navController: NavController = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val bookmarksUseCase: BookmarksUseCase = mockk(relaxed = true)
    private val context: Context = mockk(relaxed = true)
    private val collectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val accountManager: FxaAccountManager = mockk(relaxed = true)
    private val activity: HomeActivity = mockk(relaxed = true)

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        store = BrowserStore(initialState = BrowserState(tabs = listOf(testTab)))
        tabsTrayStore = TabsTrayStore()
    }

    @Test
    fun `onTabTrayDismissed calls dismissTabTray on DefaultNavigationInteractor`() {
        var dismissTabTrayInvoked = false
        createInteractor(
            dismissTabTray = {
                dismissTabTrayInvoked = true
            }
        ).onTabTrayDismissed()

        assertTrue(dismissTabTrayInvoked)
        verify {
            metrics.track(Event.TabsTrayClosed)
        }
    }

    @Test
    fun `onAccountSettingsClicked calls navigation on DefaultNavigationInteractor`() {
        every { accountManager.authenticatedAccount() }.answers { mockk(relaxed = true) }

        createInteractor().onAccountSettingsClicked()

        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalAccountSettingsFragment()) }
    }

    @Test
    fun `onAccountSettingsClicked when not logged in calls navigation to turn on sync`() {
        every { accountManager.authenticatedAccount() }.answers { null }

        createInteractor().onAccountSettingsClicked()

        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalTurnOnSync()) }
    }

    @Test
    fun `onTabSettingsClicked calls navigation on DefaultNavigationInteractor`() {
        createInteractor().onTabSettingsClicked()
        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalTabSettingsFragment()) }
    }

    @Test
    fun `onOpenRecentlyClosedClicked calls navigation on DefaultNavigationInteractor`() {
        createInteractor().onOpenRecentlyClosedClicked()
        verify(exactly = 1) { navController.navigate(TabsTrayFragmentDirections.actionGlobalRecentlyClosed()) }
    }

    @Test
    fun `onCloseAllTabsClicked calls navigation on DefaultNavigationInteractor`() {
        var dismissTabTrayAndNavigateHomeInvoked = false
        createInteractor(
            dismissTabTrayAndNavigateHome = {
                dismissTabTrayAndNavigateHomeInvoked = true
            }
        ).onCloseAllTabsClicked(false)

        assertTrue(dismissTabTrayAndNavigateHomeInvoked)
    }

    @Test
    fun `onShareTabsOfType calls navigation on DefaultNavigationInteractor`() {
        createInteractor().onShareTabsOfTypeClicked(false)
        verify(exactly = 1) { navController.navigate(any<NavDirections>()) }
    }

    @Test
    fun `onShareTabs calls navigation on DefaultNavigationInteractor`() {
        createInteractor().onShareTabs(emptyList())
        verify(exactly = 1) { navController.navigate(any<NavDirections>()) }
    }

    @Test
    fun `onSaveToCollections calls navigation on DefaultNavigationInteractor`() {
        mockkStatic("org.mozilla.fenix.collections.CollectionsDialogKt")

        every { any<CollectionsDialog>().show(any()) } answers { }
        createInteractor().onSaveToCollections(emptyList())
        verify(exactly = 1) { metrics.track(Event.TabsTraySaveToCollectionPressed) }

        unmockkStatic("org.mozilla.fenix.collections.CollectionsDialogKt")
    }

    @Test
    fun `onBookmarkTabs calls navigation on DefaultNavigationInteractor`() = runBlockingTest {
        var showBookmarkSnackbarInvoked = false
        createInteractor(
            showBookmarkSnackbar = {
                showBookmarkSnackbarInvoked = true
            }
        ).onSaveToBookmarks(listOf(createTrayTab()))

        coVerify(exactly = 1) { bookmarksUseCase.addBookmark(any(), any(), any()) }
        assertTrue(showBookmarkSnackbarInvoked)
    }

    @Test
    fun `onSyncedTabsClicked sets metrics and opens browser`() {
        val tab = mockk<SyncTab>()
        val entry = mockk<TabEntry>()

        every { tab.active() }.answers { entry }
        every { entry.url }.answers { "https://mozilla.org" }

        var dismissTabTrayInvoked = false
        createInteractor(
            dismissTabTray = {
                dismissTabTrayInvoked = true
            }
        ).onSyncedTabClicked(tab)

        assertTrue(dismissTabTrayInvoked)
        verifyOrder {
            metrics.track(Event.SyncedTabOpened)

            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromTabsTray
            )
        }
    }

    @Suppress("LongParameterList")
    private fun createInteractor(
        dismissTabTray: () -> Unit = { },
        dismissTabTrayAndNavigateHome: (String) -> Unit = { _ -> },
        showCollectionSnackbar: (Int, Boolean, Long?) -> Unit = { _, _, _ -> },
        showBookmarkSnackbar: (Int) -> Unit = { _ -> }
    ): NavigationInteractor {
        return DefaultNavigationInteractor(
            context,
            activity,
            store,
            navController,
            metrics,
            dismissTabTray,
            dismissTabTrayAndNavigateHome,
            bookmarksUseCase,
            tabsTrayStore,
            collectionStorage,
            showCollectionSnackbar,
            showBookmarkSnackbar,
            accountManager,
            testDispatcher
        )
    }
}
