/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.concept.tabstray.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultTabTrayControllerTest {
    private val activity: HomeActivity = mockk(relaxed = true)
    private val profiler: Profiler? = mockk(relaxed = true)
    private val navController: NavController = mockk()
    private val browsingModeManager: BrowsingModeManager = mockk(relaxed = true)
    private val dismissTabTray: (() -> Unit) = mockk(relaxed = true)
    private val dismissTabTrayAndNavigateHome: ((String) -> Unit) = mockk(relaxed = true)
    private val registerCollectionStorageObserver: (() -> Unit) = mockk(relaxed = true)
    private val showChooseCollectionDialog: ((List<TabSessionState>) -> Unit) = mockk(relaxed = true)
    private val showAddNewCollectionDialog: ((List<TabSessionState>) -> Unit) = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val bookmarksStorage: BookmarksStorage = mockk(relaxed = true)
    private val tabCollection: TabCollection = mockk()
    private val cachedTabCollections: List<TabCollection> = listOf(tabCollection)
    private val currentDestination: NavDestination = mockk(relaxed = true)
    private val tabTrayFragmentStore: TabTrayDialogFragmentStore = mockk(relaxed = true)
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    private val showUndoSnackbarForTabs: (() -> Unit) = mockk(relaxed = true)
    private val showBookmarksSavedSnackbar: (() -> Unit) = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    private lateinit var controller: DefaultTabTrayController

    private val tab1 = createTab(url = "http://firefox.com", id = "5678")
    private val tab2 = createTab(url = "http://mozilla.org", id = "1234")

    @Before
    fun setUp() {
        mockkStatic("org.mozilla.fenix.ext.SessionManagerKt")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2), selectedTabId = tab2.id
            )
        )

        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections
        every { navController.navigate(any<NavDirections>()) } just Runs
        every { navController.currentDestination } returns currentDestination
        every { currentDestination.id } returns R.id.browserFragment
        every { tabCollection.title } returns "Collection title"

        controller = DefaultTabTrayController(
            activity = activity,
            profiler = profiler,
            browserStore = store,
            browsingModeManager = browsingModeManager,
            tabCollectionStorage = tabCollectionStorage,
            bookmarksStorage = bookmarksStorage,
            ioScope = TestCoroutineScope(),
            metrics = metrics,
            navController = navController,
            tabsUseCases = tabsUseCases,
            dismissTabTray = dismissTabTray,
            dismissTabTrayAndNavigateHome = dismissTabTrayAndNavigateHome,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            tabTrayDialogFragmentStore = tabTrayFragmentStore,
            selectTabUseCase = selectTabUseCase,
            showChooseCollectionDialog = showChooseCollectionDialog,
            showAddNewCollectionDialog = showAddNewCollectionDialog,
            showUndoSnackbarForTabs = showUndoSnackbarForTabs,
            showBookmarksSnackbar = showBookmarksSavedSnackbar
        )
    }

    @Test
    fun handleTabSettingsClicked() {
        controller.handleTabSettingsClicked()

        verify {
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalTabSettingsFragment()
            )
        }
    }

    @Test
    fun onNewTabTapped() {
        controller.handleNewTabTapped(private = false)

        verifyOrder {
            browsingModeManager.mode = BrowsingMode.fromBoolean(false)
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true
                )
            )
            dismissTabTray()
        }

        controller.handleNewTabTapped(private = true)

        verifyOrder {
            browsingModeManager.mode = BrowsingMode.fromBoolean(true)
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true
                )
            )
            dismissTabTray()
        }
    }

    @Test
    fun onTabTrayDismissed() {
        controller.handleTabTrayDismissed()

        verify {
            dismissTabTray()
        }
    }

    @Test
    fun onShareTabsClicked() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot)) } just Runs

        controller.handleShareTabsOfTypeClicked(private = false)

        verify {
            navController.navigate(capture(navDirectionsSlot))
        }

        assertTrue(navDirectionsSlot.isCaptured)
        assertEquals(R.id.action_global_shareFragment, navDirectionsSlot.captured.actionId)
    }

    @Test
    fun onCloseAllTabsClicked() {
        controller.handleCloseAllTabsClicked(private = false)

        verify {
            dismissTabTrayAndNavigateHome(any())
        }
    }

    @Test
    fun onSyncedTabClicked() {
        controller.handleSyncedTabClicked(mockk(relaxed = true))

        verify {
            activity.openToBrowserAndLoad(any(), true, BrowserDirection.FromTabTray)
        }
    }

    @Test
    fun handleBackPressed() {
        every { tabTrayFragmentStore.state.mode } returns TabTrayDialogFragmentState.Mode.MultiSelect(
            setOf()
        )
        controller.handleBackPressed()
        verify {
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
        }
    }

    @Test
    fun onModeRequested() {
        val mode = TabTrayDialogFragmentState.Mode.MultiSelect(
            setOf()
        )
        every { tabTrayFragmentStore.state.mode } returns mode
        controller.onModeRequested()
        verify {
            tabTrayFragmentStore.state.mode
        }
    }

    @Test
    fun handleAddSelectedTab() {
        val tab = Tab("1234", "mozilla.org")
        controller.handleAddSelectedTab(tab)
        verify {
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.AddItemForCollection(tab))
        }
    }

    @Test
    fun handleRemoveSelectedTab() {
        val tab = Tab("1234", "mozilla.org")
        controller.handleRemoveSelectedTab(tab)
        verify {
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.RemoveItemForCollection(tab))
        }
    }

    @Test
    fun handleOpenTab() {
        val tab = Tab("1234", "mozilla.org")
        controller.handleOpenTab(tab)
        verify {
            selectTabUseCase.invoke(tab.id)
        }
    }

    @Test
    fun handleEnterMultiselect() {
        controller.handleEnterMultiselect()
        verify {
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.EnterMultiSelectMode)
        }
    }

    @Test
    fun onSaveToCollectionClicked() {
        val tab = Tab(tab2.id, tab2.content.url)

        controller.handleSaveToCollectionClicked(setOf(tab))

        verify {
            metrics.track(Event.TabsTraySaveToCollectionPressed)
            registerCollectionStorageObserver()
            showChooseCollectionDialog(listOf(tab2))
        }
    }

    @Test
    fun handleShareSelectedTabs() {
        val tab = Tab("1234", "mozilla.org")
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot)) } just Runs

        controller.handleShareSelectedTabsClicked(setOf(tab))

        verify {
            navController.navigate(capture(navDirectionsSlot))
        }

        assertTrue(navDirectionsSlot.isCaptured)
        assertEquals(R.id.action_global_shareFragment, navDirectionsSlot.captured.actionId)
    }

    @Test
    fun handleDeleteSelectedTabs() {
        val tab = Tab("1234", "mozilla.org")

        controller.handleDeleteSelectedTabs(setOf(tab))
        verify {
            tabsUseCases.removeTabs(listOf(tab.id))
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
            showUndoSnackbarForTabs()
        }
    }

    @Test
    fun handleBookmarkSelectedTabs() {
        val tab = Tab("1234", "mozilla.org")
        coEvery { bookmarksStorage.getBookmarksWithUrl("mozilla.org") } returns listOf()

        controller.handleBookmarkSelectedTabs(setOf(tab))
        verify {
            tabTrayFragmentStore.dispatch(TabTrayDialogFragmentAction.ExitMultiSelectMode)
            showBookmarksSavedSnackbar()
        }
    }

    @Test
    fun handleSetUpAutoCloseTabsClicked() {
        controller.handleGoToTabsSettingClicked()
        val directions = TabTrayDialogFragmentDirections.actionGlobalTabSettingsFragment()

        verify {
            navController.navigate(directions)
        }
    }
}
