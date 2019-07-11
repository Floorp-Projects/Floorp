/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.support.ktx.kotlin.isUrl
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.searchEngineManager
import org.mozilla.fenix.search.awesomebar.AwesomeBarInteractor
import org.mozilla.fenix.search.toolbar.ToolbarInteractor

/**
 * Interactor for the search screen
 * Provides implementations for the AwesomeBarView and ToolbarView
 */
class SearchInteractor(
    private val context: Context,
    private val navController: NavController,
    private val store: SearchStore
) : AwesomeBarInteractor, ToolbarInteractor {

    data class UserTypingCheck(var ranOnTextChanged: Boolean, var userHasTyped: Boolean)

    private val userTypingCheck = UserTypingCheck(false, !store.state.showShortcutEnginePicker)

    override fun onUrlCommitted(url: String) {
        if (url.isNotBlank()) {
            (context as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = store.state.session == null,
                from = BrowserDirection.FromSearch,
                engine = store.state.searchEngineSource.searchEngine
            )

            val event = if (url.isUrl()) {
                Event.EnteredUrl(false)
            } else {
                createSearchEvent(store.state.searchEngineSource.searchEngine, false)
            }

            context.metrics.track(event)
        }
    }

    override fun onEditingCanceled() {
        navController.navigateUp()
    }

    override fun onTextChanged(text: String) {
        store.dispatch(SearchAction.UpdateQuery(text))

        if (userTypingCheck.ranOnTextChanged && !userTypingCheck.userHasTyped) {
            store.dispatch(SearchAction.ShowSearchShortcutEnginePicker(false))
            turnOnStartedTyping()
        }

        userTypingCheck.ranOnTextChanged = true
    }

    override fun onUrlTapped(url: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch
        )

        context.metrics.track(Event.EnteredUrl(false))
    }

    override fun onSearchTermsTapped(searchTerms: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = searchTerms,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch,
            engine = store.state.searchEngineSource.searchEngine,
            forceSearch = true
        )

        val event = createSearchEvent(store.state.searchEngineSource.searchEngine, true)
        context.metrics.track(event)
    }

    override fun onSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        store.dispatch(SearchAction.SearchShortcutEngineSelected(searchEngine))
        context.metrics.track(Event.SearchShortcutSelected(searchEngine.name))
    }

    override fun onClickSearchEngineSettings() {
        val directions = SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment()
        navController.navigate(directions)
    }

    fun turnOnStartedTyping() {
        userTypingCheck.ranOnTextChanged = true
        userTypingCheck.userHasTyped = true
    }

    override fun onExistingSessionSelected(session: Session) {
        val directions = SearchFragmentDirections.actionSearchFragmentToBrowserFragment(null)
        navController.nav(R.id.searchFragment, directions)
        context.components.core.sessionManager.select(session)
    }

    private fun createSearchEvent(engine: SearchEngine, isSuggestion: Boolean): Event.PerformedSearch {
        val isShortcut = engine != context.searchEngineManager.defaultSearchEngine

        val engineSource =
            if (isShortcut) Event.PerformedSearch.EngineSource.Shortcut(engine)
            else Event.PerformedSearch.EngineSource.Default(engine)

        val source =
            if (isSuggestion) Event.PerformedSearch.EventSource.Suggestion(engineSource)
            else Event.PerformedSearch.EventSource.Action(engineSource)

        return Event.PerformedSearch(source)
    }
}
