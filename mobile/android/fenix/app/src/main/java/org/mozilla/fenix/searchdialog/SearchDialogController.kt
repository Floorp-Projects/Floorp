/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.searchdialog

import android.content.Intent
import androidx.navigation.NavController
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.ktx.kotlin.isUrl
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.crashes.CrashListActivity
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.search.SearchController
import org.mozilla.fenix.search.SearchFragmentAction
import org.mozilla.fenix.search.SearchFragmentStore
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings

@Suppress("TooManyFunctions", "LongParameterList")
class SearchDialogController(
    private val activity: HomeActivity,
    private val sessionManager: SessionManager,
    private val store: SearchFragmentStore,
    private val navController: NavController,
    private val settings: Settings,
    private val metrics: MetricController,
    private val clearToolbarFocus: () -> Unit
) : SearchController {

    override fun handleUrlCommitted(url: String) {
        when (url) {
            "about:crashes" -> {
                // The list of past crashes can be accessed via "settings > about", but desktop and
                // fennec users may be used to navigating to "about:crashes". So we intercept this here
                // and open the crash list activity instead.
                activity.startActivity(Intent(activity, CrashListActivity::class.java))
            }
            "moz://a" -> openSearchOrUrl(SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.MANIFESTO))
            else -> if (url.isNotBlank()) {
                openSearchOrUrl(url)
            }
        }
    }

    private fun openSearchOrUrl(url: String) {
        activity.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = store.state.tabId == null,
            from = BrowserDirection.FromSearchDialog,
            engine = store.state.searchEngineSource.searchEngine
        )

        val event = if (url.isUrl()) {
            Event.EnteredUrl(false)
        } else {
            settings.incrementActiveSearchCount()

            val searchAccessPoint = when (store.state.searchAccessPoint) {
                Event.PerformedSearch.SearchAccessPoint.NONE -> Event.PerformedSearch.SearchAccessPoint.ACTION
                else -> store.state.searchAccessPoint
            }

            searchAccessPoint?.let { sap ->
                MetricsUtils.createSearchEvent(
                    store.state.searchEngineSource.searchEngine,
                    activity,
                    sap
                )
            }
        }

        event?.let { metrics.track(it) }
    }

    override fun handleEditingCancelled() {
        clearToolbarFocus()
    }

    override fun handleTextChanged(text: String) {
        // Display the search shortcuts on each entry of the search fragment (see #5308)
        val textMatchesCurrentUrl = store.state.url == text
        val textMatchesCurrentSearch = store.state.searchTerms == text

        store.dispatch(SearchFragmentAction.UpdateQuery(text))
        store.dispatch(
            SearchFragmentAction.ShowSearchShortcutEnginePicker(
                (textMatchesCurrentUrl || textMatchesCurrentSearch || text.isEmpty()) &&
                        settings.shouldShowSearchShortcuts
            )
        )
        store.dispatch(
            SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt(
                text.isNotEmpty() &&
                        activity.browsingModeManager.mode.isPrivate &&
                        !settings.shouldShowSearchSuggestionsInPrivate &&
                        !settings.showSearchSuggestionsInPrivateOnboardingFinished
            )
        )
    }

    override fun handleUrlTapped(url: String) {
        clearToolbarFocus()

        activity.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = store.state.tabId == null,
            from = BrowserDirection.FromSearchDialog
        )

        metrics.track(Event.EnteredUrl(false))
    }

    override fun handleSearchTermsTapped(searchTerms: String) {
        settings.incrementActiveSearchCount()
        clearToolbarFocus()

        activity.openToBrowserAndLoad(
            searchTermOrURL = searchTerms,
            newTab = store.state.tabId == null,
            from = BrowserDirection.FromSearchDialog,
            engine = store.state.searchEngineSource.searchEngine,
            forceSearch = true
        )

        val searchAccessPoint = when (store.state.searchAccessPoint) {
            Event.PerformedSearch.SearchAccessPoint.NONE -> Event.PerformedSearch.SearchAccessPoint.SUGGESTION
            else -> store.state.searchAccessPoint
        }

        val event = searchAccessPoint?.let { sap ->
            MetricsUtils.createSearchEvent(
                store.state.searchEngineSource.searchEngine,
                activity,
                sap
            )
        }
        event?.let { metrics.track(it) }
    }

    override fun handleSearchShortcutEngineSelected(searchEngine: SearchEngine) {
        store.dispatch(SearchFragmentAction.SearchShortcutEngineSelected(searchEngine))
        val isCustom =
            CustomSearchEngineStore.isCustomSearchEngine(activity, searchEngine.identifier)
        metrics.track(Event.SearchShortcutSelected(searchEngine, isCustom))
    }

    override fun handleSearchShortcutsButtonClicked() {
        val isOpen = store.state.showSearchShortcuts
        store.dispatch(SearchFragmentAction.ShowSearchShortcutEnginePicker(!isOpen))
    }

    override fun handleClickSearchEngineSettings() {
        val directions = SearchDialogFragmentDirections.actionGlobalSearchEngineFragment()
        navController.navigateSafe(R.id.searchDialogFragment, directions)
    }

    override fun handleExistingSessionSelected(session: Session) {
        clearToolbarFocus()
        sessionManager.select(session)
        activity.openToBrowser(
            from = BrowserDirection.FromSearchDialog
        )
    }

    override fun handleExistingSessionSelected(tabId: String) {
        val session = sessionManager.findSessionById(tabId)
        if (session != null) {
            handleExistingSessionSelected(session)
        }
    }
}
