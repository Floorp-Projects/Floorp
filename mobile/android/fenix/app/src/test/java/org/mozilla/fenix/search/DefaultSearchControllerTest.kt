/*  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
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
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.searchEngineManager
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.whatsnew.clear

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DefaultSearchControllerTest {

    private val activity: HomeActivity = mockk(relaxed = true)
    private val store: SearchFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val defaultSearchEngine: SearchEngine? = mockk(relaxed = true)
    private val searchEngine: SearchEngine = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val clearToolbarFocus: (() -> Unit) = mockk(relaxed = true)

    private lateinit var controller: DefaultSearchController
    private lateinit var settings: Settings

    @Before
    fun setUp() {
        every { activity.searchEngineManager.defaultSearchEngine } returns defaultSearchEngine
        every { store.state.tabId } returns "test-tab-id"
        every { store.state.searchEngineSource.searchEngine } returns searchEngine
        every { activity.metrics } returns metrics
        every { activity.components.core.sessionManager } returns sessionManager

        controller = DefaultSearchController(
            activity = activity,
            store = store,
            navController = navController,
            clearToolbarFocus = clearToolbarFocus
        )

        settings = testContext.settings().apply { testContext.settings().clear() }
    }

    @Test
    fun handleUrlCommitted() {
        val url = "https://www.google.com/"

        controller.handleUrlCommitted(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearch,
                engine = searchEngine
            )
        }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleCrashesUrlCommitted() {
        val url = "about:crashes"

        controller.handleUrlCommitted(url)

        verify { activity.startActivity(any()) }
    }

    @Test
    fun handleMozillaUrlCommitted() {
        val url = "moz://a"

        controller.handleUrlCommitted(url)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.MANIFESTO),
                newTab = false,
                from = BrowserDirection.FromSearch,
                engine = searchEngine
            )
        }
        verify { metrics.track(Event.EnteredUrl(false)) }
    }

    @Test
    fun handleEditingCancelled() = runBlockingTest {
        controller = DefaultSearchController(
            activity = activity,
            store = store,
            navController = navController,
            clearToolbarFocus = clearToolbarFocus
        )

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
        testContext.settings().preferences
                .edit()
                .putBoolean(testContext.getString(R.string.pref_key_show_search_shortcuts), true)
                .apply()

        controller.handleTextChanged(text)

        verify { store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(true)) }
    }

    @Test
    fun `show search shortcuts when setting enabled AND query equals url`() {
        val text = "mozilla.org"
        every { store.state.url } returns "mozilla.org"
        testContext.settings().preferences
                .edit()
                .putBoolean(testContext.getString(R.string.pref_key_show_search_shortcuts), true)
                .apply()

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

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = false,
                from = BrowserDirection.FromSearch
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
                from = BrowserDirection.FromSearch,
                engine = searchEngine,
                forceSearch = true
            )
        }
    }

    @Test
    fun handleSearchShortcutEngineSelected() {
        val searchEngine: SearchEngine = mockk(relaxed = true)

        controller.handleSearchShortcutEngineSelected(searchEngine)

        verify { store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine)) }
        verify { metrics.track(Event.SearchShortcutSelected(searchEngine, false)) }
    }

    @Test
    fun handleClickSearchEngineSettings() {
        val directions: NavDirections =
            SearchFragmentDirections.actionGlobalSearchEngineFragment()

        controller.handleClickSearchEngineSettings()

        verify { navController.navigateSafe(R.id.searchEngineFragment, directions) }
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
        val session: Session = mockk(relaxed = true)

        controller.handleExistingSessionSelected(session)

        verify { sessionManager.select(session) }
        verify { activity.openToBrowser(from = BrowserDirection.FromSearch) }
    }
}
