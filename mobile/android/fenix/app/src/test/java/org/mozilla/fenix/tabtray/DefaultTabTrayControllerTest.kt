/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.profiler.Profiler
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
import org.mozilla.fenix.ext.sessionsOfType

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultTabTrayControllerTest {
    private val activity: HomeActivity = mockk(relaxed = true)
    private val profiler: Profiler? = mockk(relaxed = true)
    private val navController: NavController = mockk()
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val browsingModeManager: BrowsingModeManager = mockk(relaxed = true)
    private val dismissTabTray: (() -> Unit) = mockk(relaxed = true)
    private val dismissTabTrayAndNavigateHome: ((String) -> Unit) = mockk(relaxed = true)
    private val registerCollectionStorageObserver: (() -> Unit) = mockk(relaxed = true)
    private val showChooseCollectionDialog: ((List<Session>) -> Unit) = mockk(relaxed = true)
    private val showAddNewCollectionDialog: ((List<Session>) -> Unit) = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val tabCollection: TabCollection = mockk()
    private val cachedTabCollections: List<TabCollection> = listOf(tabCollection)
    private val currentDestination: NavDestination = mockk(relaxed = true)
    private val tabTrayFragmentStore: TabTrayDialogFragmentStore = mockk(relaxed = true)
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase = mockk(relaxed = true)

    private lateinit var controller: DefaultTabTrayController

    private val session = Session(
        "mozilla.org",
        true
    )

    private val nonPrivateSession = Session(
        "mozilla.org",
        false
    )

    @Before
    fun setUp() {
        mockkStatic("org.mozilla.fenix.ext.SessionManagerKt")

        every { sessionManager.sessionsOfType(private = true) } returns listOf(session).asSequence()
        every { sessionManager.sessionsOfType(private = false) } returns listOf(nonPrivateSession).asSequence()
        every { sessionManager.createSessionSnapshot(any()) } returns SessionManager.Snapshot.Item(
            session
        )
        every { sessionManager.findSessionById("1234") } returns session
        every { sessionManager.remove(any()) } just Runs
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections
        every { sessionManager.selectedSession } returns nonPrivateSession
        every { navController.navigate(any<NavDirections>()) } just Runs
        every { navController.currentDestination } returns currentDestination
        every { currentDestination.id } returns R.id.browserFragment
        every { tabCollection.title } returns "Collection title"

        controller = DefaultTabTrayController(
            activity = activity,
            profiler = profiler,
            sessionManager = sessionManager,
            browsingModeManager = browsingModeManager,
            tabCollectionStorage = tabCollectionStorage,
            navController = navController,
            dismissTabTray = dismissTabTray,
            dismissTabTrayAndNavigateHome = dismissTabTrayAndNavigateHome,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            tabTrayDialogFragmentStore = tabTrayFragmentStore,
            selectTabUseCase = selectTabUseCase,
            showChooseCollectionDialog = showChooseCollectionDialog,
            showAddNewCollectionDialog = showAddNewCollectionDialog
        )
    }

    @Test
    fun onNewTabTapped() {
        controller.onNewTabTapped(private = false)

        verifyOrder {
            browsingModeManager.mode = BrowsingMode.fromBoolean(false)
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true
                )
            )
            dismissTabTray()
        }

        controller.onNewTabTapped(private = true)

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
        controller.onTabTrayDismissed()

        verify {
            dismissTabTray()
        }
    }

    @Test
    fun onShareTabsClicked() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot)) } just Runs

        controller.onShareTabsClicked(private = false)

        verify {
            navController.navigate(capture(navDirectionsSlot))
        }

        assertTrue(navDirectionsSlot.isCaptured)
        assertEquals(R.id.action_global_shareFragment, navDirectionsSlot.captured.actionId)
    }

    @Test
    fun onCloseAllTabsClicked() {
        controller.onCloseAllTabsClicked(private = false)

        verify {
            dismissTabTrayAndNavigateHome(any())
        }
    }

    @Test
    fun onSyncedTabClicked() {
        controller.onSyncedTabClicked(mockk(relaxed = true))

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
        val tab = Tab("1234", "mozilla.org")

        controller.onSaveToCollectionClicked(setOf(tab))
        verify {
            registerCollectionStorageObserver()
            showChooseCollectionDialog(listOf(session))
        }
    }
}
