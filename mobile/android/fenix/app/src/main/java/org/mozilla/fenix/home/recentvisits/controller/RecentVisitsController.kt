/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.controller

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.feature.tabs.TabsUseCases.SelectOrAddUseCase
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight

/**
 * All possible updates following user interactions with the "Recent visits" section from the Home screen.
 */
interface RecentVisitsController {

    /**
     * Callback for when the "Show all" link is clicked.
     */
    fun handleHistoryShowAllClicked()

    /**
     * Callback for when the user clicks on a specific [RecentHistoryHighlight].
     *
     * @param recentHistoryHighlight The just clicked [RecentHistoryHighlight].
     */
    fun handleRecentHistoryHighlightClicked(recentHistoryHighlight: RecentHistoryHighlight)

    /**
     * Callback for when the user removes a certain [RecentHistoryHighlight].
     *
     * @param highlightUrl Url of the [RecentHistoryHighlight] to remove.
     */
    fun handleRemoveRecentHistoryHighlight(highlightUrl: String)
}

/**
 * The default implementation of [RecentVisitsController].
 */
class DefaultRecentVisitsController(
    private val store: BrowserStore,
    private val appStore: AppStore,
    private val selectOrAddTabUseCase: SelectOrAddUseCase,
    private val navController: NavController,
    private val storage: HistoryMetadataStorage,
    private val scope: CoroutineScope,
) : RecentVisitsController {

    /**
     * Shows the history fragment.
     */
    override fun handleHistoryShowAllClicked() {
        dismissSearchDialogIfDisplayed()
        navController.navigate(
            HomeFragmentDirections.actionGlobalHistoryFragment()
        )
    }

    /**
     * Switch to an already open tab for [recentHistoryHighlight] if one exists or
     * create a new tab in which to load this item's URL.
     *
     * @param recentHistoryHighlight the just clicked [RecentHistoryHighlight] to open in browser.
     */
    override fun handleRecentHistoryHighlightClicked(recentHistoryHighlight: RecentHistoryHighlight) {
        selectOrAddTabUseCase.invoke(recentHistoryHighlight.url)
        navController.navigate(R.id.browserFragment)
    }

    /**
     * Removes a [RecentHistoryHighlight] with the given title from the homescreen.
     *
     * @param highlightUrl The title of the [RecentHistoryHighlight] to be removed.
     */
    override fun handleRemoveRecentHistoryHighlight(highlightUrl: String) {
        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(highlightUrl))
        scope.launch {
            storage.deleteHistoryMetadataForUrl(highlightUrl)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    fun dismissSearchDialogIfDisplayed() {
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }
}
