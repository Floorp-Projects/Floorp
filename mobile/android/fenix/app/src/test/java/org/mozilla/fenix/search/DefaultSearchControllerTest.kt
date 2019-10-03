/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.searchEngineManager
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.whatsnew.clear
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DefaultSearchControllerTest {

    private val context: HomeActivity = mockk(relaxed = true)
    private val store: SearchFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val defaultSearchEngine: SearchEngine? = mockk(relaxed = true)
    private val session: Session? = mockk(relaxed = true)
    private val searchEngine: SearchEngine = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)

    private lateinit var controller: DefaultSearchController
    private lateinit var settings: Settings

    @Before
    fun setUp() {
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

        settings = testContext.settings().apply { testContext.settings().clear() }
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
    fun `do not show search shortcuts when setting disabled AND query empty`() {
        testContext.settings().preferences
            .edit()
            .putBoolean(testContext.getString(R.string.pref_key_show_search_shortcuts), false)
            .apply()

        assertFalse(testContext.settings().shouldShowSearchShortcuts)

        val text = ""

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
    }

    @Test
    fun `do not show search shortcuts when setting disabled AND query non-empty`() {
        testContext.settings().preferences
            .edit()
            .putBoolean(testContext.getString(R.string.pref_key_show_search_shortcuts), false)
            .apply()

        assertFalse(testContext.settings().shouldShowSearchShortcuts)

        val text = "mozilla"

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(false)) }
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

        verify { store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine)) }
        verify { metrics.track(Event.SearchShortcutSelected(searchEngine.name)) }
    }

    @Test
    fun handleClickSearchEngineSettings() {
        val directions: NavDirections = SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment()

        controller.handleClickSearchEngineSettings()

        verify { navController.navigate(directions) }
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
