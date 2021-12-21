/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.controller

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.feature.tabs.TabsUseCases.SelectOrAddUseCase
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentAction.RemoveRecentHistoryHighlight
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.library.history.toHistoryMetadata

/**
 * All possible updates following user interactions with the "Recent visits" section from the Home screen.
 */
interface RecentVisitsController {

    /**
     * Callback for when the "Show all" link is clicked.
     */
    fun handleHistoryShowAllClicked()

    /**
     * Callback for when the user clicks on a specific [RecentHistoryGroup].
     *
     * @param recentHistoryGroup The just clicked [RecentHistoryGroup].
     */
    fun handleRecentHistoryGroupClicked(recentHistoryGroup: RecentHistoryGroup)

    /**
     * Callback for when the user removes a certain [RecentHistoryGroup].
     *
     * @param groupTitle Title of the [RecentHistoryGroup] to remove.
     */
    fun handleRemoveRecentHistoryGroup(groupTitle: String)

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
    private val homeStore: HomeFragmentStore,
    private val selectOrAddTabUseCase: SelectOrAddUseCase,
    private val navController: NavController,
    private val storage: HistoryMetadataStorage,
    private val scope: CoroutineScope,
    private val metrics: MetricController
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
     * Navigates to the history metadata group fragment to display the group.
     *
     * @param recentHistoryGroup The [RecentHistoryGroup] to which to navigate to.
     */
    override fun handleRecentHistoryGroupClicked(recentHistoryGroup: RecentHistoryGroup) {
        navController.navigate(
            HomeFragmentDirections.actionGlobalHistoryMetadataGroup(
                title = recentHistoryGroup.title,
                historyMetadataItems = recentHistoryGroup.historyMetadata
                    .mapIndexed { index, item -> item.toHistoryMetadata(index) }.toTypedArray()
            )
        )
    }

    /**
     * Removes a [RecentHistoryGroup] with the given title from the homescreen.
     *
     * @param groupTitle The title of the [RecentHistoryGroup] to be removed.
     */
    override fun handleRemoveRecentHistoryGroup(groupTitle: String) {
        // We want to update the UI right away in response to user action without waiting for the IO.
        // First, dispatch actions that will clean up search groups in the two stores that have
        // metadata-related state.
        store.dispatch(HistoryMetadataAction.DisbandSearchGroupAction(searchTerm = groupTitle))
        homeStore.dispatch(HomeFragmentAction.DisbandSearchGroupAction(searchTerm = groupTitle))
        // Then, perform the expensive IO work of removing search groups from storage.
        scope.launch {
            storage.deleteHistoryMetadata(groupTitle)
        }
        metrics.track(Event.RecentSearchesGroupDeleted)
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
        homeStore.dispatch(RemoveRecentHistoryHighlight(highlightUrl))
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
