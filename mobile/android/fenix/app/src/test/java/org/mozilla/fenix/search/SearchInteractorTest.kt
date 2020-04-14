/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.Context
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore.PREF_FILE_SEARCH_ENGINES
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.searchEngineManager

@ExperimentalCoroutinesApi
class SearchInteractorTest {

    private val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
    private val clearToolbarFocus = { }

    @Test
    fun onUrlCommitted() {
        val context: HomeActivity = mockk(relaxed = true)
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()
        val searchEngineManager: SearchEngineManager = mockk(relaxed = true)
        val searchEngine = SearchEngineSource.Default(mockk(relaxed = true))
        val searchAccessPoint: Event.PerformedSearch.SearchAccessPoint = mockk(relaxed = true)

        every { context.metrics } returns mockk(relaxed = true)
        every { context.searchEngineManager } returns searchEngineManager
        every { context.openToBrowserAndLoad(any(), any(), any(), any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null
        every { state.searchEngineSource } returns searchEngine
        every { state.searchAccessPoint } returns searchAccessPoint

        every {
            context.getSharedPreferences(
                PREF_FILE_SEARCH_ENGINES,
                Context.MODE_PRIVATE
            )
        } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk(),
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)

        interactor.onUrlCommitted("test")

        verify {
            context.openToBrowserAndLoad(
                searchTermOrURL = "test",
                newTab = true,
                from = BrowserDirection.FromSearch,
                engine = searchEngine.searchEngine
            )
        }
    }

    @Test
    fun onEditingCanceled() = runBlockingTest {
        val navController: NavController = mockk(relaxed = true)
        val store: SearchFragmentStore = mockk(relaxed = true)

        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            mockk(),
            store,
            navController,
            this,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)

        interactor.onEditingCanceled()
        advanceTimeBy(DefaultSearchController.KEYBOARD_ANIMATION_DELAY)

        verify {
            clearToolbarFocus()
            navController.popBackStack()
        }
    }

    @Test
    fun onTextChanged() {
        val store: SearchFragmentStore = mockk(relaxed = true)
        val context: HomeActivity = mockk(relaxed = true)

        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk(),
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)

        interactor.onTextChanged("test")

        verify { store.dispatch(SearchFragmentAction.UpdateQuery("test")) }
        verify { store.dispatch(SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(false)) }
    }

    @Test
    fun onUrlTapped() {
        val context: HomeActivity = mockk()
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()
        val searchEngine = SearchEngineSource.Default(mockk(relaxed = true))

        every { context.metrics } returns mockk(relaxed = true)
        every { context.openToBrowserAndLoad(any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null
        every { state.searchEngineSource } returns searchEngine

        every {
            context.getSharedPreferences(
                PREF_FILE_SEARCH_ENGINES,
                Context.MODE_PRIVATE
            )
        } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk(),
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)

        interactor.onUrlTapped("test")

        verify {
            context.openToBrowserAndLoad(
                "test",
                true,
                BrowserDirection.FromSearch
            )
        }
    }

    @Test
    fun onSearchTermsTapped() {
        val context: HomeActivity = mockk(relaxed = true)
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()
        val searchEngineManager: SearchEngineManager = mockk(relaxed = true)
        val searchEngine = SearchEngineSource.Default(mockk(relaxed = true))
        val searchAccessPoint: Event.PerformedSearch.SearchAccessPoint = mockk(relaxed = true)

        every { context.metrics } returns mockk(relaxed = true)
        every { context.searchEngineManager } returns searchEngineManager
        every { context.openToBrowserAndLoad(any(), any(), any(), any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null
        every { state.searchEngineSource } returns searchEngine
        every { state.searchAccessPoint } returns searchAccessPoint

        every {
            context.getSharedPreferences(
                PREF_FILE_SEARCH_ENGINES,
                Context.MODE_PRIVATE
            )
        } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk(),
            lifecycleScope,
            clearToolbarFocus
        )

        val interactor = SearchInteractor(searchController)

        interactor.onSearchTermsTapped("test")
        verify {
            context.openToBrowserAndLoad(
                searchTermOrURL = "test",
                newTab = true,
                from = BrowserDirection.FromSearch,
                engine = searchEngine.searchEngine,
                forceSearch = true
            )
        }
    }

    @Test
    fun onSearchShortcutEngineSelected() {
        val context: HomeActivity = mockk(relaxed = true)

        every { context.metrics } returns mockk(relaxed = true)

        val store: SearchFragmentStore = mockk(relaxed = true)
        val state: SearchFragmentState = mockk(relaxed = true)

        every { store.state } returns state

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk(),
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)
        val searchEngine: SearchEngine = mockk(relaxed = true)

        interactor.onSearchShortcutEngineSelected(searchEngine)

        verify { store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine)) }
    }

    @Test
    fun onSearchShortcutsButtonClicked() {
        val searchController: SearchController = mockk(relaxed = true)
        val interactor = SearchInteractor(searchController)

        interactor.onSearchShortcutsButtonClicked()

        verify { searchController.handleSearchShortcutsButtonClicked() }
    }

    @Test
    fun onClickSearchEngineSettings() {
        val navController: NavController = mockk()
        val store: SearchFragmentStore = mockk()

        every { store.state } returns mockk(relaxed = true)
        every { navController.currentDestination?.id } returns R.id.searchFragment

        val searchController: SearchController = DefaultSearchController(
            mockk(),
            store,
            navController,
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)

        every { navController.navigate(any() as NavDirections) } just Runs

        interactor.onClickSearchEngineSettings()

        verify {
            navController.navigateSafe(
                R.id.searchFragment,
                SearchFragmentDirections.actionGlobalSearchEngineFragment()
            )
        }
    }

    @Test
    fun onExistingSessionSelected() {
        val navController: NavController = mockk(relaxed = true)
        val context: HomeActivity = mockk(relaxed = true)
        val applicationContext: FenixApplication = mockk(relaxed = true)
        every { context.applicationContext } returns applicationContext
        val store: SearchFragmentStore = mockk()
        every { context.openToBrowser(any(), any()) } just Runs

        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            navController,
            lifecycleScope,
            clearToolbarFocus
        )
        val interactor = SearchInteractor(searchController)
        val session = Session("http://mozilla.org", false)

        interactor.onExistingSessionSelected(session)

        verify {
            context.openToBrowser(BrowserDirection.FromSearch)
        }
    }
}
