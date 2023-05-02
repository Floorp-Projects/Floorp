/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.MockKAnnotations
import io.mockk.coVerify
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.runs
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.collections.CollectionsDialog
import org.mozilla.fenix.collections.show
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.ext.potentialInactiveTabs
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.utils.Settings
import java.util.concurrent.TimeUnit

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class DefaultTabsTrayControllerTest {
    @MockK(relaxed = true)
    private lateinit var trayStore: TabsTrayStore

    @MockK(relaxed = true)
    private lateinit var browserStore: BrowserStore

    @MockK(relaxed = true)
    private lateinit var browsingModeManager: BrowsingModeManager

    @MockK(relaxed = true)
    private lateinit var navController: NavController

    @MockK(relaxed = true)
    private lateinit var profiler: Profiler

    @MockK(relaxed = true)
    private lateinit var navigationInteractor: NavigationInteractor

    @MockK(relaxed = true)
    private lateinit var tabsUseCases: TabsUseCases

    @MockK(relaxed = true)
    private lateinit var activity: HomeActivity

    private val appStore: AppStore = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)

    private val bookmarksUseCase: BookmarksUseCase = mockk(relaxed = true)
    private val collectionStorage: TabCollectionStorage = mockk(relaxed = true)

    private val coroutinesTestRule: MainCoroutineRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val chain: RuleChain = RuleChain.outerRule(coroutinesTestRule).around(gleanTestRule)

    @Before
    fun setup() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `GIVEN private mode WHEN the fab is clicked THEN a profile marker is added for the operations executed`() {
        profiler = spyk(profiler) {
            every { getProfilerTime() } returns Double.MAX_VALUE
        }

        assertNull(TabsTray.newPrivateTabTapped.testGetValue())

        createController().handlePrivateTabsFabClick()

        assertNotNull(TabsTray.newPrivateTabTapped.testGetValue())

        verifyOrder {
            profiler.getProfilerTime()
            navController.navigate(
                TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
            navigationInteractor.onTabTrayDismissed()
            profiler.addMarker(
                "DefaultTabTrayController.onNewTabTapped",
                Double.MAX_VALUE,
            )
        }
    }

    @Test
    fun `GIVEN normal mode WHEN the fab is clicked THEN a profile marker is added for the operations executed`() {
        profiler = spyk(profiler) {
            every { getProfilerTime() } returns Double.MAX_VALUE
        }

        createController().handleNormalTabsFabClick()

        verifyOrder {
            profiler.getProfilerTime()
            navController.navigate(
                TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
            )
            navigationInteractor.onTabTrayDismissed()
            profiler.addMarker(
                "DefaultTabTrayController.onNewTabTapped",
                Double.MAX_VALUE,
            )
        }
    }

    @Test
    fun `GIVEN private mode WHEN the fab is clicked THEN Event#NewPrivateTabTapped is added to telemetry`() {
        assertNull(TabsTray.newPrivateTabTapped.testGetValue())

        createController().handlePrivateTabsFabClick()

        assertNotNull(TabsTray.newPrivateTabTapped.testGetValue())
    }

    @Test
    fun `GIVEN normal mode WHEN the fab is clicked THEN Event#NewTabTapped is added to telemetry`() {
        assertNull(TabsTray.newTabTapped.testGetValue())

        createController().handleNormalTabsFabClick()

        assertNotNull(TabsTray.newTabTapped.testGetValue())
    }

    @Test
    fun `GIVEN the user is on the synced tabs page WHEN the fab is clicked THEN fire off a sync action`() {
        every { trayStore.state.syncing } returns false

        createController().handleSyncedTabsFabClick()

        verify { trayStore.dispatch(TabsTrayAction.SyncNow) }
    }

    @Test
    fun `GIVEN the user is on the synced tabs page and there is already an active sync WHEN the fab is clicked THEN no action should be taken`() {
        every { trayStore.state.syncing } returns true

        createController().handleSyncedTabsFabClick()

        verify(exactly = 0) { trayStore.dispatch(TabsTrayAction.SyncNow) }
    }

    @Test
    fun `WHEN handleTabDeletion is called THEN Event#ClosedExistingTab is added to telemetry`() {
        val tab: TabSessionState = mockk { every { content.private } returns true }
        assertNull(TabsTray.closedExistingTab.testGetValue())

        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.findTab(any()) } returns tab
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(tab)

            createController().handleTabDeletion("testTabId", "unknown")
            assertNotNull(TabsTray.closedExistingTab.testGetValue())
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN active private download WHEN handleTabDeletion is called for the last private tab THEN showCancelledDownloadWarning is called`() {
        var showCancelledDownloadWarningInvoked = false
        val controller = spyk(
            createController(
                showCancelledDownloadWarning = { _, _, _ ->
                    showCancelledDownloadWarningInvoked = true
                },
            ),
        )
        val tab: TabSessionState = mockk { every { content.private } returns true }
        every { browserStore.state } returns mockk()
        every { browserStore.state.downloads } returns mapOf(
            "1" to DownloadState(
                "https://mozilla.org/download",
                private = true,
                destinationDirectory = "Download",
                status = DownloadState.Status.DOWNLOADING,
            ),
        )
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.findTab(any()) } returns tab
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(tab)
            every { browserStore.state.selectedTabId } returns "testTabId"

            controller.handleTabDeletion("testTabId", "unknown")

            assertTrue(showCancelledDownloadWarningInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=true THEN it scrolls to that position with smoothScroll`() {
        var selectTabPositionInvoked = false
        createController(
            selectTabPosition = { position, smoothScroll ->
                assertEquals(3, position)
                assertTrue(smoothScroll)
                selectTabPositionInvoked = true
            },
        ).handleTrayScrollingToPosition(3, true)

        assertTrue(selectTabPositionInvoked)
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=true THEN it emits an action for the tray page of that tab position`() {
        createController().handleTrayScrollingToPosition(33, true)

        verify { trayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(33))) }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=false THEN it emits an action for the tray page of that tab position`() {
        createController().handleTrayScrollingToPosition(44, true)

        verify { trayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(44))) }
    }

    @Test
    fun `GIVEN already on browserFragment WHEN handleNavigateToBrowser is called THEN the tray is dismissed`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment

        var dismissTrayInvoked = false
        createController(dismissTray = { dismissTrayInvoked = true }).handleNavigateToBrowser()

        assertTrue(dismissTrayInvoked)
        verify(exactly = 0) { navController.popBackStack() }
        verify(exactly = 0) { navController.popBackStack(any<Int>(), any()) }
        verify(exactly = 0) { navController.navigate(any<Int>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>(), any<NavOptions>()) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called THEN the tray is dismissed and popBackStack is executed`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment + 1
        every { navController.popBackStack(R.id.browserFragment, false) } returns true

        var dismissTrayInvoked = false
        createController(dismissTray = { dismissTrayInvoked = true }).handleNavigateToBrowser()

        assertTrue(dismissTrayInvoked)
        verify { navController.popBackStack(R.id.browserFragment, false) }
        verify(exactly = 0) { navController.navigate(any<Int>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>(), any<NavOptions>()) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called and popBackStack fails THEN it navigates to browserFragment`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment + 1
        every { navController.popBackStack(R.id.browserFragment, false) } returns false

        var dismissTrayInvoked = false
        createController(dismissTray = { dismissTrayInvoked = true }).handleNavigateToBrowser()

        assertTrue(dismissTrayInvoked)
        verify { navController.popBackStack(R.id.browserFragment, false) }
        verify { navController.navigate(R.id.browserFragment) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called and popBackStack succeeds THEN the method finishes`() {
        every { navController.popBackStack(R.id.browserFragment, false) } returns true

        var dismissTrayInvoked = false
        createController(dismissTray = { dismissTrayInvoked = true }).handleNavigateToBrowser()

        assertTrue(dismissTrayInvoked)
        verify(exactly = 1) { navController.popBackStack(R.id.browserFragment, false) }
        verify(exactly = 0) { navController.navigate(R.id.browserFragment) }
    }

    @Test
    fun `GIVEN more tabs opened WHEN handleTabDeletion is called THEN that tab is removed and an undo snackbar is shown`() {
        val tab: TabSessionState = mockk {
            every { content } returns mockk()
            every { content.private } returns true
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.findTab(any()) } returns tab
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(tab, mockk())

            var showUndoSnackbarForTabInvoked = false
            createController(
                showUndoSnackbarForTab = {
                    assertTrue(it)
                    showUndoSnackbarForTabInvoked = true
                },
            ).handleTabDeletion("22")

            verify { tabsUseCases.removeTab("22") }
            assertTrue(showUndoSnackbarForTabInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN only one tab opened WHEN handleTabDeletion is called THEN that it navigates to home where the tab will be removed`() {
        var showUndoSnackbarForTabInvoked = false
        val controller = spyk(createController(showUndoSnackbarForTab = { showUndoSnackbarForTabInvoked = true }))
        val tab: TabSessionState = mockk {
            every { content } returns mockk()
            every { content.private } returns true
        }
        every { browserStore.state } returns mockk()
        try {
            val testTabId = "33"
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.findTab(any()) } returns tab
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(tab)
            every { browserStore.state.selectedTabId } returns testTabId

            controller.handleTabDeletion(testTabId)

            verify { controller.dismissTabsTrayAndNavigateHome(testTabId) }
            verify(exactly = 0) { tabsUseCases.removeTab(any()) }
            assertFalse(showUndoSnackbarForTabInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close all private tabs THEN that it navigates to home where that tabs will be removed and shows undo snackbar`() {
        var showUndoSnackbarForTabInvoked = false
        val controller = spyk(
            createController(
                showUndoSnackbarForTab = {
                    assertTrue(it)
                    showUndoSnackbarForTabInvoked = true
                },
            ),
        )

        val privateTab = createTab(url = "url", private = true)

        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.deleteMultipleTabs(listOf(privateTab, mockk()))

            verify { controller.dismissTabsTrayAndNavigateHome(HomeFragment.ALL_PRIVATE_TABS) }
            assertTrue(showUndoSnackbarForTabInvoked)
            verify(exactly = 0) { tabsUseCases.removeTabs(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close all normal tabs THEN that it navigates to home where that tabs will be removed and shows undo snackbar`() {
        var showUndoSnackbarForTabInvoked = false
        val controller = spyk(
            createController(
                showUndoSnackbarForTab = {
                    assertFalse(it)
                    showUndoSnackbarForTabInvoked = true
                },
            ),
        )

        val normalTab = createTab(url = "url", private = false)

        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.deleteMultipleTabs(listOf(normalTab, normalTab))

            verify { controller.dismissTabsTrayAndNavigateHome(HomeFragment.ALL_NORMAL_TABS) }
            verify(exactly = 0) { tabsUseCases.removeTabs(any()) }
            assertTrue(showUndoSnackbarForTabInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close some private tabs THEN that it uses tabsUseCases#removeTabs and shows an undo snackbar`() {
        var showUndoSnackbarForTabInvoked = false
        val controller = spyk(createController(showUndoSnackbarForTab = { showUndoSnackbarForTabInvoked = true }))
        val privateTab = createTab(id = "42", url = "url", private = true)

        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.deleteMultipleTabs(listOf(privateTab))

            verify { tabsUseCases.removeTabs(listOf("42")) }
            verify(exactly = 0) { controller.dismissTabsTrayAndNavigateHome(any()) }
            assertTrue(showUndoSnackbarForTabInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close some normal tabs THEN that it uses tabsUseCases#removeTabs and shows an undo snackbar`() {
        var showUndoSnackbarForTabInvoked = false
        val controller = spyk(createController(showUndoSnackbarForTab = { showUndoSnackbarForTabInvoked = true }))
        val privateTab = createTab(id = "24", url = "url", private = false)

        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.deleteMultipleTabs(listOf(privateTab))

            verify { tabsUseCases.removeTabs(listOf("24")) }
            verify(exactly = 0) { controller.dismissTabsTrayAndNavigateHome(any()) }
            assertTrue(showUndoSnackbarForTabInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN one tab is selected WHEN the delete selected tabs button is clicked THEN report the telemetry and delete the tabs`() {
        val controller = spyk(createController())

        every { trayStore.state.mode.selectedTabs } returns setOf(createTab(url = "url"))
        every { controller.deleteMultipleTabs(any()) } just runs

        controller.handleDeleteSelectedTabsClicked()

        assertNotNull(TabsTray.closeSelectedTabs.testGetValue())
        val snapshot = TabsTray.closeSelectedTabs.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("1", snapshot.single().extra?.getValue("tab_count"))

        verify { trayStore.dispatch(TabsTrayAction.ExitSelectMode) }
    }

    @Test
    fun `GIVEN private mode selected WHEN sendNewTabEvent is called THEN NewPrivateTabTapped is tracked in telemetry`() {
        createController().sendNewTabEvent(true)

        assertNotNull(TabsTray.newPrivateTabTapped.testGetValue())
    }

    @Test
    fun `GIVEN normal mode selected WHEN sendNewTabEvent is called THEN NewTabTapped is tracked in telemetry`() {
        assertNull(TabsTray.newTabTapped.testGetValue())

        createController().sendNewTabEvent(false)

        assertNotNull(TabsTray.newTabTapped.testGetValue())
    }

    @Test
    fun `WHEN dismissTabsTrayAndNavigateHome is called with a specific tab id THEN tray is dismissed and navigates home is opened to delete that tab`() {
        var dismissTrayInvoked = false
        var navigateToHomeAndDeleteSessionInvoked = false
        createController(
            dismissTray = {
                dismissTrayInvoked = true
            },
            navigateToHomeAndDeleteSession = {
                assertEquals("randomId", it)
                navigateToHomeAndDeleteSessionInvoked = true
            },
        ).dismissTabsTrayAndNavigateHome("randomId")

        assertTrue(dismissTrayInvoked)
        assertTrue(navigateToHomeAndDeleteSessionInvoked)
    }

    @Test
    fun `WHEN a synced tab is clicked THEN the metrics are reported and the tab is opened`() {
        val tab = mockk<Tab>()
        val entry = mockk<TabEntry>()
        assertNull(Events.syncedTabOpened.testGetValue())

        every { tab.active() }.answers { entry }
        every { entry.url }.answers { "https://mozilla.org" }

        var dismissTabTrayInvoked = false
        createController(
            dismissTray = {
                dismissTabTrayInvoked = true
            },
        ).handleSyncedTabClicked(tab)

        assertTrue(dismissTabTrayInvoked)
        assertNotNull(Events.syncedTabOpened.testGetValue())

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromTabsTray,
            )
        }
    }

    @Test
    fun `GIVEN no tabs selected and the user is not in multi select mode WHEN the user long taps a tab THEN that tab will become selected`() {
        trayStore = TabsTrayStore()
        val controller = spyk(createController())
        val tab1 = TabSessionState(
            id = "1",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val tab2 = TabSessionState(
            id = "2",
            content = ContentState(
                url = "www.google.com",
            ),
        )
        trayStore.dispatch(TabsTrayAction.ExitSelectMode)
        trayStore.waitUntilIdle()

        controller.handleMultiSelectClicked(tab1, "Tabs tray")
        verify(exactly = 1) { controller.handleTabSelected(tab1, "Tabs tray") }

        controller.handleMultiSelectClicked(tab2, "Tabs tray")
        verify(exactly = 1) { controller.handleTabSelected(tab2, "Tabs tray") }
    }

    @Test
    fun `GIVEN the user is in multi select mode and a tab is selected WHEN the user taps the selected tab THEN the tab will become unselected`() {
        trayStore = TabsTrayStore()
        val tab1 = TabSessionState(
            id = "1",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val tab2 = TabSessionState(
            id = "2",
            content = ContentState(
                url = "www.google.com",
            ),
        )
        val controller = spyk(createController())
        trayStore.dispatch(TabsTrayAction.EnterSelectMode)
        trayStore.dispatch(TabsTrayAction.AddSelectTab(tab1))
        trayStore.dispatch(TabsTrayAction.AddSelectTab(tab2))
        trayStore.waitUntilIdle()

        controller.handleMultiSelectClicked(tab1, "Tabs tray")
        verify(exactly = 1) { controller.handleTabUnselected(tab1) }

        controller.handleMultiSelectClicked(tab2, "Tabs tray")
        verify(exactly = 1) { controller.handleTabUnselected(tab2) }
    }

    @Test
    fun `GIVEN at least a tab is selected and the user is in multi select mode WHEN the user taps a tab THEN that tab will become selected`() {
        val middleware = CaptureActionsMiddleware<TabsTrayState, TabsTrayAction>()
        trayStore = TabsTrayStore(middlewares = listOf(middleware))
        trayStore.dispatch(TabsTrayAction.EnterSelectMode)
        trayStore.waitUntilIdle()
        val controller = spyk(createController())
        val tab1 = TabSessionState(
            id = "1",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val tab2 = TabSessionState(
            id = "2",
            content = ContentState(
                url = "www.google.com",
            ),
        )

        trayStore.dispatch(TabsTrayAction.EnterSelectMode)
        trayStore.dispatch(TabsTrayAction.AddSelectTab(tab1))
        trayStore.waitUntilIdle()

        controller.handleMultiSelectClicked(tab2, "Tabs tray")

        middleware.assertLastAction(TabsTrayAction.AddSelectTab::class) {
            assertEquals(tab2, it.tab)
        }
    }

    @Test
    fun `GIVEN the user selects only the current tab WHEN the user forces tab to be inactive THEN tab does not become inactive`() {
        val currentTab = TabSessionState(content = mockk(), id = "currentTab", createdAt = 11)
        val secondTab = TabSessionState(content = mockk(), id = "secondTab", createdAt = 22)
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(currentTab, secondTab),
                selectedTabId = currentTab.id,
            ),
        )

        every { trayStore.state.mode.selectedTabs } returns setOf(currentTab)

        createController().handleForceSelectedTabsAsInactiveClicked(numDays = 5)

        browserStore.waitUntilIdle()

        val updatedCurrentTab = browserStore.state.tabs.first { it.id == currentTab.id }
        assertEquals(updatedCurrentTab, currentTab)
        val updatedSecondTab = browserStore.state.tabs.first { it.id == secondTab.id }
        assertEquals(updatedSecondTab, secondTab)
    }

    @Test
    fun `GIVEN the user selects multiple tabs including the current tab WHEN the user forces them all to be inactive THEN all but current tab become inactive`() {
        val currentTab = TabSessionState(content = mockk(), id = "currentTab", createdAt = 11)
        val secondTab = TabSessionState(content = mockk(), id = "secondTab", createdAt = 22)
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(currentTab, secondTab),
                selectedTabId = currentTab.id,
            ),
        )

        every { trayStore.state.mode.selectedTabs } returns setOf(currentTab, secondTab)

        createController().handleForceSelectedTabsAsInactiveClicked(numDays = 5)

        browserStore.waitUntilIdle()

        val updatedCurrentTab = browserStore.state.tabs.first { it.id == currentTab.id }
        assertEquals(updatedCurrentTab, currentTab)
        val updatedSecondTab = browserStore.state.tabs.first { it.id == secondTab.id }
        assertNotEquals(updatedSecondTab, secondTab)
        val expectedTime = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(5)
        // Account for System.currentTimeMillis() giving different values in test vs the system under test
        // and also for the waitUntilIdle to block for even hundreds of milliseconds.
        assertTrue(updatedSecondTab.lastAccess in (expectedTime - 5000)..expectedTime)
        assertTrue(updatedSecondTab.createdAt in (expectedTime - 5000)..expectedTime)
    }

    @Test
    fun `GIVEN no value is provided for inactive days WHEN forcing tabs as inactive THEN set their last active time 15 days ago and exit multi selection`() {
        val controller = spyk(createController())
        every { trayStore.state.mode.selectedTabs } returns setOf(createTab(url = "https://mozilla.org"))
        every { browserStore.state.selectedTabId } returns "test"

        controller.handleForceSelectedTabsAsInactiveClicked()

        verify { controller.handleForceSelectedTabsAsInactiveClicked(numDays = 15L) }

        verify { trayStore.dispatch(TabsTrayAction.ExitSelectMode) }
    }

    fun `WHEN the inactive tabs section is expanded THEN the expanded telemetry event should be reported`() {
        val controller = createController()

        assertNull(TabsTray.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTray.inactiveTabsCollapsed.testGetValue())

        controller.handleInactiveTabsHeaderClicked(expanded = true)

        assertNotNull(TabsTray.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTray.inactiveTabsCollapsed.testGetValue())
    }

    @Test
    fun `WHEN the inactive tabs section is collapsed THEN the collapsed telemetry event should be reported`() {
        val controller = createController()

        assertNull(TabsTray.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTray.inactiveTabsCollapsed.testGetValue())

        controller.handleInactiveTabsHeaderClicked(expanded = false)

        assertNull(TabsTray.inactiveTabsExpanded.testGetValue())
        assertNotNull(TabsTray.inactiveTabsCollapsed.testGetValue())
    }

    @Test
    fun `WHEN the inactive tabs auto-close feature prompt is dismissed THEN update settings and report the telemetry event`() {
        val controller = spyk(createController())

        assertNull(TabsTray.autoCloseDimissed.testGetValue())

        controller.handleInactiveTabsAutoCloseDialogDismiss()

        assertNotNull(TabsTray.autoCloseDimissed.testGetValue())
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
    }

    @Test
    fun `WHEN the inactive tabs auto-close feature prompt is accepted THEN update settings and report the telemetry event`() {
        val controller = spyk(createController())

        assertNull(TabsTray.autoCloseTurnOnClicked.testGetValue())

        controller.handleEnableInactiveTabsAutoCloseClicked()

        assertNotNull(TabsTray.autoCloseTurnOnClicked.testGetValue())

        verify { settings.closeTabsAfterOneMonth = true }
        verify { settings.closeTabsAfterOneWeek = false }
        verify { settings.closeTabsAfterOneDay = false }
        verify { settings.manuallyCloseTabs = false }
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
    }

    @Test
    fun `WHEN an inactive tab is selected THEN report the telemetry event and open the tab`() {
        val controller = spyk(createController())
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )

        every { controller.handleTabSelected(any(), any()) } just runs

        assertNull(TabsTray.openInactiveTab.testGetValue())

        controller.handleInactiveTabClicked(tab)

        assertNotNull(TabsTray.openInactiveTab.testGetValue())

        verify { controller.handleTabSelected(tab, TrayPagerAdapter.INACTIVE_TABS_FEATURE_NAME) }
    }

    @Test
    fun `WHEN an inactive tab is closed THEN report the telemetry event and delete the tab`() {
        val controller = spyk(createController())
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )

        every { controller.handleTabDeletion(any(), any()) } just runs

        assertNull(TabsTray.closeInactiveTab.testGetValue())

        controller.handleCloseInactiveTabClicked(tab)

        assertNotNull(TabsTray.closeInactiveTab.testGetValue())

        verify { controller.handleTabDeletion(tab.id, TrayPagerAdapter.INACTIVE_TABS_FEATURE_NAME) }
    }

    @Test
    fun `WHEN all inactive tabs are closed THEN perform the deletion and report the telemetry event and show a Snackbar`() {
        var showSnackbarInvoked = false
        val controller = createController(
            showUndoSnackbarForTab = {
                showSnackbarInvoked = true
            },
        )
        val inactiveTab: TabSessionState = mockk {
            every { lastAccess } returns maxActiveTime
            every { createdAt } returns 0
            every { id } returns "24"
            every { content } returns mockk {
                every { private } returns false
            }
        }

        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state } returns mockk()
            every { browserStore.state.potentialInactiveTabs } returns listOf(inactiveTab)
            assertNull(TabsTray.closeAllInactiveTabs.testGetValue())

            controller.handleDeleteAllInactiveTabsClicked()

            verify { tabsUseCases.removeTabs(listOf("24")) }
            assertNotNull(TabsTray.closeAllInactiveTabs.testGetValue())
            assertTrue(showSnackbarInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    fun `WHEN a tab is selected THEN report the metric, update the state, and open the browser`() {
        val controller = spyk(createController())
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val source = TrayPagerAdapter.INACTIVE_TABS_FEATURE_NAME

        every { controller.handleNavigateToBrowser() } just runs

        assertNull(TabsTray.openedExistingTab.testGetValue())

        controller.handleTabSelected(tab, source)

        assertNotNull(TabsTray.openedExistingTab.testGetValue())
        val snapshot = TabsTray.openedExistingTab.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals(source, snapshot.single().extra?.getValue("opened_existing_tab"))

        verify { tabsUseCases.selectTab(tab.id) }
        verify { controller.handleNavigateToBrowser() }
    }

    fun `WHEN a tab is selected without a source THEN report the metric with an unknown source, update the state, and open the browser`() {
        val controller = spyk(createController())
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        )
        val sourceText = "unknown"

        every { controller.handleNavigateToBrowser() } just runs

        assertNull(TabsTray.openedExistingTab.testGetValue())

        controller.handleTabSelected(tab, null)

        assertNotNull(TabsTray.openedExistingTab.testGetValue())
        val snapshot = TabsTray.openedExistingTab.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals(sourceText, snapshot.single().extra?.getValue("opened_existing_tab"))

        verify { tabsUseCases.selectTab(tab.id) }
        verify { controller.handleNavigateToBrowser() }
    }

    @Test
    fun `GIVEN a private tab is open and selected with a normal tab also open WHEN the private tab is closed and private home page shown and normal tab is selected from tabs tray THEN normal tab is displayed  `() {
        val normalTab = TabSessionState(
            content = ContentState(url = "https://simulate.com", private = false),
            id = "normalTab",
        )
        val privateTab = TabSessionState(
            content = ContentState(url = "https://mozilla.com", private = true),
            id = "privateTab",
        )
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(normalTab, privateTab),
            ),
        )
        browsingModeManager = spyk(
            DefaultBrowsingModeManager(
                _mode = BrowsingMode.Private,
                settings = settings,
                modeDidChange = mockk(relaxed = true),
            ),
        )
        val controller = spyk(createController())

        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            browserStore.dispatch(TabListAction.SelectTabAction(privateTab.id)).joinBlocking()
            controller.handleTabSelected(privateTab, null)

            assertEquals(privateTab.id, browserStore.state.selectedTabId)
            assertEquals(true, browsingModeManager.mode.isPrivate)

            controller.handleTabDeletion("privateTab")
            browserStore.dispatch(TabListAction.SelectTabAction(normalTab.id)).joinBlocking()
            controller.handleTabSelected(normalTab, null)

            assertEquals(normalTab.id, browserStore.state.selectedTabId)
            assertEquals(false, browsingModeManager.mode.isPrivate)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN a normal tab is selected WHEN the last private tab is deleted THEN that private tab is removed and an undo snackbar is shown and original normal tab is still displayed`() {
        val currentTab = TabSessionState(content = ContentState(url = "https://simulate.com", private = false), id = "currentTab")
        val privateTab = TabSessionState(content = ContentState(url = "https://mozilla.com", private = true), id = "privateTab")
        var showUndoSnackbarForTabInvoked = false
        var navigateToHomeAndDeleteSessionInvoked = false
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(currentTab, privateTab),
                selectedTabId = currentTab.id,
            ),
        )
        val controller = spyk(
            createController(
                showUndoSnackbarForTab = {
                    showUndoSnackbarForTabInvoked = true
                },
                navigateToHomeAndDeleteSession = {
                    navigateToHomeAndDeleteSessionInvoked = true
                },
            ),
        )

        controller.handleTabSelected(currentTab, "source")
        controller.handleTabDeletion("privateTab")

        assertTrue(showUndoSnackbarForTabInvoked)
        assertFalse(navigateToHomeAndDeleteSessionInvoked)
    }

    @Test
    fun `GIVEN no tabs are currently selected WHEN a normal tab is long clicked THEN the tab is selected and the metric is reported`() {
        val normalTab = TabSessionState(
            content = ContentState(url = "https://simulate.com", private = false),
            id = "normalTab",
        )
        every { trayStore.state.mode.selectedTabs } returns emptySet()

        assertNull(Collections.longPress.testGetValue())

        createController().handleTabLongClick(normalTab)

        assertNotNull(Collections.longPress.testGetValue())
        verify { trayStore.dispatch(TabsTrayAction.AddSelectTab(normalTab)) }
    }

    @Test
    fun `GIVEN at least one tab is selected WHEN a normal tab is long clicked THEN the long click is ignored`() {
        val normalTabClicked = TabSessionState(
            content = ContentState(url = "https://simulate.com", private = false),
            id = "normalTab",
        )
        val alreadySelectedTab = TabSessionState(
            content = ContentState(url = "https://simulate.com", private = false),
            id = "selectedTab",
        )
        every { trayStore.state.mode.selectedTabs } returns setOf(alreadySelectedTab)

        createController().handleTabLongClick(normalTabClicked)

        assertNull(Collections.longPress.testGetValue())
        verify(exactly = 0) { trayStore.dispatch(any()) }
    }

    @Test
    fun `WHEN a private tab is long clicked THEN the long click is ignored`() {
        val privateTab = TabSessionState(
            content = ContentState(url = "https://simulate.com", private = true),
            id = "privateTab",
        )

        createController().handleTabLongClick(privateTab)

        assertNull(Collections.longPress.testGetValue())
        verify(exactly = 0) { trayStore.dispatch(any()) }
    }

    @Test
    fun `GIVEN one tab is selected WHEN the share button is clicked THEN report the telemetry and navigate away`() {
        every { trayStore.state.mode.selectedTabs } returns setOf(createTab(url = "https://mozilla.org"))

        createController().handleShareSelectedTabsClicked()

        verify(exactly = 1) { navController.navigate(any<NavDirections>()) }

        assertNotNull(TabsTray.shareSelectedTabs.testGetValue())
        val snapshot = TabsTray.shareSelectedTabs.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("1", snapshot.single().extra?.getValue("tab_count"))
    }

    @Test
    fun `GIVEN one tab is selected WHEN the add selected tabs to collection button is clicked THEN report the telemetry and show the collections dialog`() {
        mockkStatic("org.mozilla.fenix.collections.CollectionsDialogKt")

        val controller = spyk(createController())
        every { controller.showCollectionsDialog(any()) } just runs

        every { trayStore.state.mode.selectedTabs } returns setOf(createTab(url = "https://mozilla.org"))
        every { any<CollectionsDialog>().show(any()) } answers { }
        assertNull(TabsTray.saveToCollection.testGetValue())

        controller.handleAddSelectedTabsToCollectionClicked()

        assertNotNull(TabsTray.saveToCollection.testGetValue())

        unmockkStatic("org.mozilla.fenix.collections.CollectionsDialogKt")
    }

    @Test
    fun `GIVEN one tab is selected WHEN the save selected tabs to bookmarks button is clicked THEN report the telemetry and show a snackbar`() = runTestOnMain {
        var showBookmarkSnackbarInvoked = false

        every { trayStore.state.mode.selectedTabs } returns setOf(createTab(url = "https://mozilla.org"))

        createController(
            showBookmarkSnackbar = {
                showBookmarkSnackbarInvoked = true
            },
        ).handleBookmarkSelectedTabsClicked()

        coVerify(exactly = 1) { bookmarksUseCase.addBookmark(any(), any(), any()) }
        assertTrue(showBookmarkSnackbarInvoked)

        assertNotNull(TabsTray.bookmarkSelectedTabs.testGetValue())
        val snapshot = TabsTray.bookmarkSelectedTabs.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("1", snapshot.single().extra?.getValue("tab_count"))
    }

    private fun createController(
        navigateToHomeAndDeleteSession: (String) -> Unit = { },
        selectTabPosition: (Int, Boolean) -> Unit = { _, _ -> },
        dismissTray: () -> Unit = { },
        showUndoSnackbarForTab: (Boolean) -> Unit = { _ -> },
        showCancelledDownloadWarning: (Int, String?, String?) -> Unit = { _, _, _ -> },
        showCollectionSnackbar: (Int, Boolean) -> Unit = { _, _ -> },
        showBookmarkSnackbar: (Int) -> Unit = { _ -> },
    ): DefaultTabsTrayController {
        return DefaultTabsTrayController(
            activity = activity,
            appStore = appStore,
            tabsTrayStore = trayStore,
            browserStore = browserStore,
            settings = settings,
            browsingModeManager = browsingModeManager,
            navController = navController,
            navigateToHomeAndDeleteSession = navigateToHomeAndDeleteSession,
            profiler = profiler,
            navigationInteractor = navigationInteractor,
            tabsUseCases = tabsUseCases,
            bookmarksUseCase = bookmarksUseCase,
            collectionStorage = collectionStorage,
            ioDispatcher = testDispatcher,
            selectTabPosition = selectTabPosition,
            dismissTray = dismissTray,
            showUndoSnackbarForTab = showUndoSnackbarForTab,
            showCancelledDownloadWarning = showCancelledDownloadWarning,
            showCollectionSnackbar = showCollectionSnackbar,
            showBookmarkSnackbar = showBookmarkSnackbar,
        )
    }
}
