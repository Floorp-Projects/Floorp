/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.searchEngineManager

/**
 * An interface that handles the view manipulation of the Search, triggered by the Interactor
 */
interface SearchController {
    fun handleUrlCommitted(url: String)
    fun handleEditingCancelled()
    fun handleTextChanged(text: String)
    fun handleUrlTapped(url: String)
    fun handleSearchTermsTapped(searchTerms: String)
    fun handleSearchShortcutEngineSelected(searchEngine: SearchEngine)
    fun handleClickSearchEngineSettings()
    fun handleExistingSessionSelected(session: Session)
    fun handleSearchShortcutsButtonClicked()
}

class DefaultSearchController(
    private val context: Context,
    private val store: SearchFragmentStore,
    private val navController: NavController
) : SearchController {

    override fun handleUrlCommitted(url: String) {
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
            if (CustomSearchEngineStore.isCustomSearchEngine(
                    context,
                    store.state.searchEngineSource.searchEngine.identifier
                )
            ) {
                context.components.analytics.metrics.track(Event.SearchWithCustomEngine)
            }
        }
    }

    override fun handleEditingCancelled() {
        navController.navigateUp()
    }

    override fun handleTextChanged(text: String) {
        store.dispatch(SearchFragmentAction.UpdateQuery(text))
        store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(
            text.isEmpty() && context.settings().shouldShowSearchShortcuts
        ))
        store.dispatch(SearchFragmentAction.ShowSearchSuggestionsHint(
            text.isNotEmpty() &&
                    (context as HomeActivity).browsingModeManager.mode.isPrivate &&
                    !context.settings().shouldShowSearchSuggestionsInPrivate &&
                    !context.settings().showSearchSuggestionsInPrivateOnboardingFinished
        ))
    }

    override fun handleUrlTapped(url: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch
        )

        context.metrics.track(Event.EnteredUrl(false))
        if (CustomSearchEngineStore.isCustomSearchEngine(
                context,
                store.state.searchEngineSource.searchEngine.identifier
            )
        ) {
            context.components.analytics.metrics.track(Event.SearchWithCustomEngine)
        }
    }

    override fun handleSearchTermsTapped(searchTerms: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = searchTerms,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch,
            engine = store.state.searchEngineSource.searchEngine,
            forceSearch = true
        )

        val event = createSearchEvent(store.state.searchEngineSource.searchEngine, true)
        context.metrics.track(event)
        if (CustomSearchEngineStore.isCustomSearchEngine(
                context,
                store.state.searchEngineSource.searchEngine.identifier
            )
        ) {
            context.components.analytics.metrics.track(Event.SearchWithCustomEngine)
        }
    }

    override fun handleSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine))
        context.metrics.track(Event.SearchShortcutSelected(searchEngine.name))
    }

    override fun handleSearchShortcutsButtonClicked() {
        val isOpen = store.state.showSearchShortcuts
        store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(!isOpen))
    }

    override fun handleClickSearchEngineSettings() {
        val directions = SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment()
        navController.navigate(directions)
    }

    override fun handleExistingSessionSelected(session: Session) {
        val directions = SearchFragmentDirections.actionSearchFragmentToBrowserFragment(null)
        navController.nav(R.id.searchFragment, directions)
        context.components.core.sessionManager.select(session)
    }

    private fun createSearchEvent(
        engine: SearchEngine,
        isSuggestion: Boolean
    ): Event.PerformedSearch {
        val isShortcut = engine != context.searchEngineManager.defaultSearchEngine
        val isCustom = CustomSearchEngineStore.isCustomSearchEngine(context, engine.identifier)

        val engineSource =
            if (isShortcut) Event.PerformedSearch.EngineSource.Shortcut(engine, isCustom)
            else Event.PerformedSearch.EngineSource.Default(engine, isCustom)

        val source =
            if (isSuggestion) Event.PerformedSearch.EventSource.Suggestion(engineSource)
            else Event.PerformedSearch.EventSource.Action(engineSource)

        return Event.PerformedSearch(source)
    }
}
