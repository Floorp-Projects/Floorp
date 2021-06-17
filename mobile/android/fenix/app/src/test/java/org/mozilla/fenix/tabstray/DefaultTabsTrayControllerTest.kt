/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.tabstray.Tab
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragment

@RunWith(FenixRobolectricTestRunner::class)
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
    private lateinit var navigateToHomeAndDeleteSession: (String) -> Unit

    @MockK(relaxed = true)
    private lateinit var profiler: Profiler

    @MockK(relaxed = true)
    private lateinit var navigationInteractor: NavigationInteractor

    @MockK(relaxed = true)
    private lateinit var metrics: MetricController

    @MockK(relaxed = true)
    private lateinit var tabsUseCases: TabsUseCases

    @MockK(relaxed = true)
    private lateinit var selectTabPosition: (Int, Boolean) -> Unit

    @MockK(relaxed = true)
    private lateinit var dismissTray: () -> Unit

    @MockK(relaxed = true)
    private lateinit var showUndoSnackbarForTab: (Boolean) -> Unit

    private lateinit var controller: DefaultTabsTrayController

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        controller = DefaultTabsTrayController(
            trayStore,
            browserStore,
            browsingModeManager,
            navController,
            navigateToHomeAndDeleteSession,
            profiler,
            navigationInteractor,
            metrics,
            tabsUseCases,
            selectTabPosition,
            dismissTray,
            showUndoSnackbarForTab
        )
    }

    @Test
    fun `GIVEN private mode WHEN handleOpeningNewTab is called THEN a profile marker is added for the operations executed`() {
        profiler = spyk(profiler) {
            every { getProfilerTime() } returns Double.MAX_VALUE
        }
        controller = DefaultTabsTrayController(
            trayStore,
            browserStore,
            browsingModeManager,
            navController,
            navigateToHomeAndDeleteSession,
            profiler,
            navigationInteractor,
            metrics,
            tabsUseCases,
            selectTabPosition,
            dismissTray,
            showUndoSnackbarForTab
        )

        controller.handleOpeningNewTab(true)

        verifyOrder {
            profiler.getProfilerTime()
            navController.navigate(
                TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
            )
            navigationInteractor.onTabTrayDismissed()
            profiler.addMarker(
                "DefaultTabTrayController.onNewTabTapped",
                Double.MAX_VALUE
            )
        }
    }

    @Test
    fun `GIVEN normal mode WHEN handleOpeningNewTab is called THEN a profile marker is added for the operations executed`() {
        profiler = spyk(profiler) {
            every { getProfilerTime() } returns Double.MAX_VALUE
        }
        controller = DefaultTabsTrayController(
            trayStore,
            browserStore,
            browsingModeManager,
            navController,
            navigateToHomeAndDeleteSession,
            profiler,
            navigationInteractor,
            metrics,
            tabsUseCases,
            selectTabPosition,
            dismissTray,
            showUndoSnackbarForTab
        )

        controller.handleOpeningNewTab(false)

        verifyOrder {
            profiler.getProfilerTime()
            navController.navigate(
                TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
            )
            navigationInteractor.onTabTrayDismissed()
            profiler.addMarker(
                "DefaultTabTrayController.onNewTabTapped",
                Double.MAX_VALUE
            )
        }
    }

    @Test
    fun `GIVEN private mode WHEN handleOpeningNewTab is called THEN Event#NewPrivateTabTapped is added to telemetry`() {
        controller.handleOpeningNewTab(true)

        verify { metrics.track(Event.NewPrivateTabTapped) }
    }

    @Test
    fun `GIVEN private mode WHEN handleOpeningNewTab is called THEN Event#NewTabTapped is added to telemetry`() {
        controller.handleOpeningNewTab(false)

        verify { metrics.track(Event.NewTabTapped) }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=true THEN it scrolls to that position with smoothScroll`() {
        controller.handleTrayScrollingToPosition(3, true)

        verify { selectTabPosition(3, true) }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=true THEN it emits an action for the tray page of that tab position`() {
        controller.handleTrayScrollingToPosition(33, true)

        verify { trayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(33))) }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=false THEN it scrolls to that position without smoothScroll`() {
        controller.handleTrayScrollingToPosition(4, false)

        verify { selectTabPosition(4, false) }
    }

    @Test
    fun `WHEN handleTrayScrollingToPosition is called with smoothScroll=false THEN it emits an action for the tray page of that tab position`() {
        controller.handleTrayScrollingToPosition(44, true)

        verify { trayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(44))) }
    }

    @Test
    fun `GIVEN already on browserFragment WHEN handleNavigateToBrowser is called THEN the tray is dismissed`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment

        controller.handleNavigateToBrowser()

        verify { dismissTray() }
        verify(exactly = 0) { navController.popBackStack() }
        verify(exactly = 0) { navController.popBackStack(any(), any()) }
        verify(exactly = 0) { navController.navigate(any<Int>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>(), any<NavOptions>()) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called THEN the tray is dismissed and popBackStack is executed`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment + 1
        every { navController.popBackStack(R.id.browserFragment, false) } returns true

        controller.handleNavigateToBrowser()

        verify { dismissTray() }
        verify { navController.popBackStack(R.id.browserFragment, false) }
        verify(exactly = 0) { navController.navigate(any<Int>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>()) }
        verify(exactly = 0) { navController.navigate(any<NavDirections>(), any<NavOptions>()) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called and popBackStack fails THEN it navigates to browserFragment`() {
        every { navController.currentDestination?.id } returns R.id.browserFragment + 1
        every { navController.popBackStack(R.id.browserFragment, false) } returns false

        controller.handleNavigateToBrowser()

        verify { dismissTray() }
        verify { navController.popBackStack(R.id.browserFragment, false) }
        verify { navController.navigate(R.id.browserFragment) }
    }

    @Test
    fun `GIVEN not already on browserFragment WHEN handleNavigateToBrowser is called and popBackStack succeeds THEN the method finishes`() {
        every { navController.popBackStack(R.id.browserFragment, false) } returns true

        controller.handleNavigateToBrowser()

        verify { dismissTray() }
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

            controller.handleTabDeletion("22")

            verify { tabsUseCases.removeTab("22") }
            verify { showUndoSnackbarForTab(true) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN only one tab opened WHEN handleTabDeletion is called THEN that it navigates to home where the tab will be removed`() {
        controller = spyk(controller)
        val tab: TabSessionState = mockk {
            every { content } returns mockk()
            every { content.private } returns true
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.findTab(any()) } returns tab
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(tab)

            controller.handleTabDeletion("33")

            verify { controller.dismissTabsTrayAndNavigateHome("33") }
            verify(exactly = 0) { tabsUseCases.removeTab(any()) }
            verify(exactly = 0) { showUndoSnackbarForTab(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close all private tabs THEN that it navigates to home where that tabs will be removed and shows undo snackbar`() {
        controller = spyk(controller)
        val privateTab: Tab = mockk {
            every { private } returns true
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.handleMultipleTabsDeletion(listOf(privateTab, mockk()))

            verify { controller.dismissTabsTrayAndNavigateHome(HomeFragment.ALL_PRIVATE_TABS) }
            verify { showUndoSnackbarForTab(true) }
            verify(exactly = 0) { tabsUseCases.removeTabs(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close all normal tabs THEN that it navigates to home where that tabs will be removed and shows undo snackbar`() {
        controller = spyk(controller)
        val normalTab: Tab = mockk {
            every { private } returns false
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.handleMultipleTabsDeletion(listOf(normalTab, normalTab))

            verify { controller.dismissTabsTrayAndNavigateHome(HomeFragment.ALL_NORMAL_TABS) }
            verify { showUndoSnackbarForTab(false) }
            verify(exactly = 0) { tabsUseCases.removeTabs(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close some private tabs THEN that it uses tabsUseCases#removeTabs and shows an undo snackbar`() {
        controller = spyk(controller)
        val privateTab: Tab = mockk {
            every { private } returns true
            every { id } returns "42"
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.handleMultipleTabsDeletion(listOf(privateTab))

            verify { tabsUseCases.removeTabs(listOf("42")) }
            verify { showUndoSnackbarForTab(true) }
            verify(exactly = 0) { controller.dismissTabsTrayAndNavigateHome(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `WHEN handleMultipleTabsDeletion is called to close some normal tabs THEN that it uses tabsUseCases#removeTabs and shows an undo snackbar`() {
        controller = spyk(controller)
        val privateTab: Tab = mockk {
            every { private } returns false
            every { id } returns "24"
        }
        every { browserStore.state } returns mockk()
        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state.getNormalOrPrivateTabs(any()) } returns listOf(mockk(), mockk())

            controller.handleMultipleTabsDeletion(listOf(privateTab))

            verify { tabsUseCases.removeTabs(listOf("24")) }
            verify { showUndoSnackbarForTab(false) }
            verify(exactly = 0) { controller.dismissTabsTrayAndNavigateHome(any()) }
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    @Test
    fun `GIVEN private mode selected WHEN sendNewTabEvent is called THEN NewPrivateTabTapped is tracked in telemetry`() {
        controller.sendNewTabEvent(true)

        verify { metrics.track(Event.NewPrivateTabTapped) }
    }

    @Test
    fun `GIVEN normal mode selected WHEN sendNewTabEvent is called THEN NewTabTapped is tracked in telemetry`() {
        controller.sendNewTabEvent(false)

        verify { metrics.track(Event.NewTabTapped) }
    }

    @Test
    fun `WHEN dismissTabsTrayAndNavigateHome is called with a spefic tab id THEN tray is dismissed and navigates home is opened to delete that tab`() {
        controller.dismissTabsTrayAndNavigateHome("randomId")

        verify { dismissTray() }
        verify { navigateToHomeAndDeleteSession("randomId") }
    }
}
