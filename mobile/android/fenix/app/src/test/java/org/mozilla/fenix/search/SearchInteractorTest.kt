/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.Context
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.searchEngineManager

class SearchInteractorTest {
    @Test
    fun onUrlCommitted() {
        val context: HomeActivity = mockk()
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()
        val searchEngineManager: SearchEngineManager = mockk(relaxed = true)
        val searchEngine = SearchEngineSource.Default(mockk())

        every { context.metrics } returns mockk(relaxed = true)
        every { context.searchEngineManager } returns searchEngineManager
        every { context.openToBrowserAndLoad(any(), any(), any(), any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null
        every { state.searchEngineSource } returns searchEngine

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk()
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
    fun onEditingCanceled() {
        val navController: NavController = mockk(relaxed = true)
        val store: SearchFragmentStore = mockk()

        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            mockk(),
            store,
            navController
        )
        val interactor = SearchInteractor(searchController)

        interactor.onEditingCanceled()

        verify {
            navController.navigateUp()
        }
    }

    @Test
    fun onTextChanged() {
        val store: SearchFragmentStore = mockk(relaxed = true)

        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            mockk(),
            store,
            mockk()
        )
        val interactor = SearchInteractor(searchController)

        interactor.onTextChanged("test")

        verify { store.dispatch(SearchFragmentAction.UpdateQuery("test")) }
    }

    @Test
    fun onUrlTapped() {
        val context: HomeActivity = mockk()
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()

        every { context.metrics } returns mockk(relaxed = true)
        every { context.openToBrowserAndLoad(any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk()
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
        val context: HomeActivity = mockk()
        val store: SearchFragmentStore = mockk()
        val state: SearchFragmentState = mockk()
        val searchEngineManager: SearchEngineManager = mockk(relaxed = true)
        val searchEngine = SearchEngineSource.Default(mockk())

        every { context.metrics } returns mockk(relaxed = true)
        every { context.searchEngineManager } returns searchEngineManager
        every { context.openToBrowserAndLoad(any(), any(), any(), any(), any(), any()) } just Runs

        every { store.state } returns state
        every { state.session } returns null
        every { state.searchEngineSource } returns searchEngine

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            mockk()
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
            mockk()
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

        val searchController: SearchController = DefaultSearchController(
            mockk(),
            store,
            navController
        )
        val interactor = SearchInteractor(searchController)

        every { navController.navigate(any() as NavDirections) } just Runs

        interactor.onClickSearchEngineSettings()

        verify {
            navController.navigate(SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment())
        }
    }

    @Test
    fun onExistingSessionSelected() {
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.searchFragment
        }
        val context: Context = mockk(relaxed = true)
        val applicationContext: FenixApplication = mockk(relaxed = true)
        every { context.applicationContext } returns applicationContext
        val store: SearchFragmentStore = mockk()
        every { store.state } returns mockk(relaxed = true)

        val searchController: SearchController = DefaultSearchController(
            context,
            store,
            navController
        )
        val interactor = SearchInteractor(searchController)
        val session = Session("http://mozilla.org", false)

        interactor.onExistingSessionSelected(session)

        verify {
            navController.navigate(
                SearchFragmentDirections.actionSearchFragmentToBrowserFragment(
                    null
                )
            )
        }
    }
}
