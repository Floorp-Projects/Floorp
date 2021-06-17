/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.appcompat.app.AlertDialog
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.MetricsUtils

import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGlobalAddonsManagementFragment
import org.mozilla.fenix.search.SearchDialogFragmentDirections.Companion.actionGlobalSearchEngineFragment
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
class SearchDialogControllerTest {

    @MockK(relaxed = true) private lateinit var activity: HomeActivity
    @MockK(relaxed = true) private lateinit var store: SearchDialogFragmentStore
    @MockK(relaxed = true) private lateinit var navController: NavController
    @MockK private lateinit var searchEngine: SearchEngine
    @MockK(relaxed = true) private lateinit var metrics: MetricController
    @MockK(relaxed = true) private lateinit var settings: Settings
    @MockK(relaxed = true) private lateinit var clearToolbarFocus: () -> Unit
    @MockK(relaxed = true) private lateinit var focusToolbar: () -> Unit
    @MockK(relaxed = true) private lateinit var clearToolbar: () -> Unit
    @MockK(relaxed = true) private lateinit var dismissDialog: () -> Unit

    private lateinit var controller: SearchDialogController
    private lateinit var middleware: CaptureActionsMiddleware<BrowserState, BrowserAction>
    private lateinit var browserStore: BrowserStore

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        mockkObject(MetricsUtils)
        middleware = CaptureActionsMiddleware()
        browserStore = BrowserStore(
            middleware = listOf(middleware)
        )
        every { store.state.tabId } returns "test-tab-id"
        every { store.state.searchEngineSource.searchEngine } returns searchEngine
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        every { MetricsUtils.createSearchEvent(searchEngine, browserStore, any()) } returns null

        val tabsUseCases = TabsUseCases(browserStore)

        controller = SearchDialogController(
            activity = activity,
            store = browserStore,
            tabsUseCases = tabsUseCases,
            fragmentStore = store,
            navController = navController,
            settings = settings,
            metrics = metrics,
            dismissDialog = dismissDialog,
            clearToolbarFocus = clearToolbarFocus,
            focusToolbar = focusToolbar,
            clearToolbar = clearToolbar
        )
    }

    @After
    fun teardown() {
        unmockkObject(MetricsUtils)
    }

    @Test
    fun handleUrlCommitted() {
        val url = "https://www.google.com/"

        controller.handleUrlCommitted(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine
            )
        }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleBlankUrlCommitted() {
        val url = ""

        controller.handleUrlCommitted(url)

        verify {
            dismissDialog()
        }
    }

    @Test
    fun handleSearchCommitted() {
        val searchTerm = "Firefox"

        controller.handleUrlCommitted(searchTerm)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = searchTerm,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine
            )
        }
    }

    @Test
    fun handleCrashesUrlCommitted() {
        val url = "about:crashes"
        every { activity.packageName } returns "org.mozilla.fenix"

        controller.handleUrlCommitted(url)

        verify {
            activity.startActivity(any())
        }
    }

    @Test
    fun handleAddonsUrlCommitted() {
        val url = "about:addons"
        val directions = actionGlobalAddonsManagementFragment()

        controller.handleUrlCommitted(url)

        verify { navController.navigate(directions) }
    }

    @Test
    fun handleMozillaUrlCommitted() {
        val url = "moz://a"

        controller.handleUrlCommitted(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.MANIFESTO),
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine
            )
        }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleEditingCancelled() = runBlockingTest {
        controller.handleEditingCancelled()

        verify {
            clearToolbarFocus()
        }
    }

    @Test
    fun handleTextChangedNonEmpty() {
        val text = "fenix"

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.UpdateQuery(text)) }
    }

    @Test
    fun handleTextChangedEmpty() {
        val text = ""

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.UpdateQuery(text)) }
    }

    @Test
    fun `show search shortcuts when setting enabled AND query empty`() {
        val text = ""
        every { settings.shouldShowSearchShortcuts } returns true

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)) }
    }

    @Test
    fun `show search shortcuts when setting enabled AND query equals url`() {
        val text = "mozilla.org"
        every { store.state.url } returns "mozilla.org"
        every { settings.shouldShowSearchShortcuts } returns true

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)) }
    }

    @Test
    fun `do not show search shortcuts when setting enabled AND query non-empty`() {
        val text = "mozilla"

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
    }

    @Test
    fun `do not show search shortcuts when setting disabled AND query empty AND url not matching query`() {
        every { settings.shouldShowSearchShortcuts } returns false

        val text = ""

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
    }

    @Test
    fun `do not show search shortcuts when setting disabled AND query non-empty`() {
        every { settings.shouldShowSearchShortcuts } returns false

        val text = "mozilla"

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
    }

    @Test
    fun handleUrlTapped() {
        val url = "https://www.google.com/"

        controller.handleUrlTapped(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearchDialog
            )
        }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleSearchTermsTapped() {
        val searchTerms = "fenix"

        controller.handleSearchTermsTapped(searchTerms)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = searchTerms,
                newTab = false,
                from = BrowserDirection.FromSearchDialog,
                engine = searchEngine,
                forceSearch = true
            )
        }
    }

    @Test
    fun handleSearchShortcutEngineSelected() {
        val searchEngine: SearchEngine = mockk(relaxed = true)

        controller.handleSearchShortcutEngineSelected(searchEngine)

        verify { focusToolbar() }
        verify { store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine)) }
        verify { metrics.track(Event.SearchShortcutSelected(searchEngine, false)) }
    }

    @Test
    fun handleClickSearchEngineSettings() {
        val directions: NavDirections = actionGlobalSearchEngineFragment()

        controller.handleClickSearchEngineSettings()

        verify { navController.navigate(directions) }
    }

    @Test
    fun handleSearchShortcutsButtonClicked_alreadyOpen() {
        every { store.state.showSearchShortcuts } returns true

        controller.handleSearchShortcutsButtonClicked()

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
    }

    @Test
    fun handleSearchShortcutsButtonClicked_notYetOpen() {
        every { store.state.showSearchShortcuts } returns false

        controller.handleSearchShortcutsButtonClicked()

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)) }
    }

    @Test
    fun handleExistingSessionSelected() {
        controller.handleExistingSessionSelected("selected")

        browserStore.waitUntilIdle()

        middleware.assertFirstAction(TabListAction.SelectTabAction::class) { action ->
            assertEquals("selected", action.tabId)
        }

        verify { activity.openToBrowser(from = BrowserDirection.FromSearchDialog) }
    }

    @Test
    fun handleExistingSessionSelected_tabId() {
        controller.handleExistingSessionSelected("tab-id")

        browserStore.waitUntilIdle()

        middleware.assertFirstAction(TabListAction.SelectTabAction::class) { action ->
            assertEquals("tab-id", action.tabId)
        }
        verify { activity.openToBrowser(from = BrowserDirection.FromSearchDialog) }
    }

    @Test
    fun `show camera permissions needed dialog`() {
        val dialogBuilder: AlertDialog.Builder = mockk(relaxed = true)

        val spyController = spyk(controller)
        every { spyController.buildDialog() } returns dialogBuilder

        spyController.handleCameraPermissionsNeeded()

        verify { dialogBuilder.show() }
    }
}
