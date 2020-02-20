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
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.SearchAccessPoint.ACTION
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.SearchAccessPoint.NONE
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.SearchAccessPoint.SUGGESTION
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings

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
                val searchAccessPoint = when (store.state.searchAccessPoint) {
                    NONE -> ACTION
                    else -> store.state.searchAccessPoint
                }

                searchAccessPoint?.let { sap ->
                    MetricsUtils.createSearchEvent(
                        store.state.searchEngineSource.searchEngine,
                        context,
                        sap
                    )
                }
            }

            event?.let { context.metrics.track(it) }
        }
    }

    override fun handleEditingCancelled() {
        navController.navigateUp()
    }

    override fun handleTextChanged(text: String) {
        store.dispatch(SearchFragmentAction.UpdateQuery(text))
        store.dispatch(
            SearchFragmentAction.ShowSearchShortcutEnginePicker(
                text.isEmpty() && context.settings().shouldShowSearchShortcuts
            )
        )
        store.dispatch(
            SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(
                text.isNotEmpty() &&
                        context.components.browsingModeManager.mode.isPrivate &&
                        !context.settings().shouldShowSearchSuggestionsInPrivate &&
                        !context.settings().showSearchSuggestionsInPrivateOnboardingFinished
            )
        )
    }

    override fun handleUrlTapped(url: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch
        )

        context.metrics.track(Event.EnteredUrl(false))
    }

    override fun handleSearchTermsTapped(searchTerms: String) {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = searchTerms,
            newTab = store.state.session == null,
            from = BrowserDirection.FromSearch,
            engine = store.state.searchEngineSource.searchEngine,
            forceSearch = true
        )

        val searchAccessPoint = when (store.state.searchAccessPoint) {
            NONE -> SUGGESTION
            else -> store.state.searchAccessPoint
        }

        val event = searchAccessPoint?.let { sap ->
            MetricsUtils.createSearchEvent(
                store.state.searchEngineSource.searchEngine,
                context,
                sap
            )
        }
        event?.let { context.metrics.track(it) }
    }

    override fun handleSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine))
        val isCustom =
            CustomSearchEngineStore.isCustomSearchEngine(context, searchEngine.identifier)
        context.metrics.track(Event.SearchShortcutSelected(searchEngine, isCustom))
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
        context.components.core.sessionManager.select(session)
        (context as HomeActivity).openToBrowser(
            from = BrowserDirection.FromSearch
        )
    }
}
