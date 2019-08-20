/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.searchEngineManager

class DefaultSearchControllerTest {

    private val context: HomeActivity = mockk(relaxed = true)
    private val store: SearchStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val defaultSearchEngine: SearchEngine? = mockk(relaxed = true)
    private val session: Session? = mockk(relaxed = true)
    private val searchEngine: SearchEngine = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)

    private lateinit var controller: DefaultSearchController

    @Before
    fun setUp() {
        every { store.state.showShortcutEnginePicker } returns false
        every { context.searchEngineManager.defaultSearchEngine } returns defaultSearchEngine
        every { store.state.session } returns session
        every { store.state.searchEngineSource.searchEngine } returns searchEngine
        every { context.metrics } returns metrics
        every { context.components.core.sessionManager } returns sessionManager

        controller = DefaultSearchController(
            context = context,
            store = store,
            navController = navController
        )
    }

    @Test
    fun handleUrlCommitted() {
        val url = "https://www.google.com/"

        controller.handleUrlCommitted(url)

        verify { context.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = session == null,
            from = BrowserDirection.FromSearch,
            engine = searchEngine
        ) }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleEditingCancelled() {
        controller.handleEditingCancelled()

        verify { navController.navigateUp() }
    }

    @Test
    fun handleTextChanged() {
        val text = "fenix"

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchAction.UpdateQuery(text)) }
        verify(inverse = true) {
            store.dispatch(SearchAction.ShowSearchShortcutEnginePicker(false))
        }
        assertTrue(controller.userTypingCheck.ranOnTextChanged)
    }

    @Test
    fun handleUrlTapped() {
        val url = "https://www.google.com/"

        controller.handleUrlTapped(url)

        verify { context.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = session == null,
            from = BrowserDirection.FromSearch
        ) }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleSearchTermsTapped() {
        val searchTerms = "fenix"

        controller.handleSearchTermsTapped(searchTerms)

        verify { context.openToBrowserAndLoad(
            searchTermOrURL = searchTerms,
            newTab = session == null,
            from = BrowserDirection.FromSearch,
            engine = searchEngine,
            forceSearch = true
        ) }
    }

    @Test
    fun handleSearchShortcutEngineSelected() {
        val searchEngine: SearchEngine = mockk(relaxed = true)

        controller.handleSearchShortcutEngineSelected(searchEngine)

        verify { store.dispatch(SearchAction.SearchShortcutEngineSelected(searchEngine)) }
        verify { metrics.track(Event.SearchShortcutSelected(searchEngine.name)) }
    }

    @Test
    fun handleClickSearchEngineSettings() {
        val directions: NavDirections = SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment()

        controller.handleClickSearchEngineSettings()

        verify { navController.navigate(directions) }
    }

    @Test
    fun handleTurnOnStartedTyping() {
        controller.handleTurnOnStartedTyping()

        assertTrue(controller.userTypingCheck.ranOnTextChanged)
        assertTrue(controller.userTypingCheck.userHasTyped)
    }

    @Test
    fun handleExistingSessionSelected() {
        val session: Session = mockk(relaxed = true)
        val directions = SearchFragmentDirections.actionSearchFragmentToBrowserFragment(null)

        controller.handleExistingSessionSelected(session)

        verify { navController.nav(R.id.searchFragment, directions) }
        verify { sessionManager.select(session) }
    }
}
