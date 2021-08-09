/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.controller

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.navigation.NavController
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.utils.Settings

/**
 * An interface that handles the view manipulation of the history metadata in the Home screen.
 */
interface HistoryMetadataController {

    /**
     * @see [HistoryMetadataInteractor.onHistoryMetadataItemClicked]
     */
    fun handleHistoryMetadataItemClicked(url: String, historyMetadata: HistoryMetadataKey)

    /**
     * @see [HistoryMetadataInteractor.onHistoryMetadataShowAllClicked]
     */
    fun handleHistoryShowAllClicked()

    /**
     * @see [HistoryMetadataInteractor.onToggleHistoryMetadataGroupExpanded]
     */
    fun handleToggleHistoryMetadataGroupExpanded(historyMetadataGroup: HistoryMetadataGroup)
}

/**
 * The default implementation of [HistoryMetadataController].
 */
class DefaultHistoryMetadataController(
    private val activity: HomeActivity,
    private val settings: Settings,
    private val homeFragmentStore: HomeFragmentStore,
    private val selectOrAddUseCase: TabsUseCases.SelectOrAddUseCase,
    private val navController: NavController
) : HistoryMetadataController {

    override fun handleHistoryMetadataItemClicked(
        url: String,
        historyMetadata: HistoryMetadataKey
    ) {
        val tabId = selectOrAddUseCase.invoke(
            url = url,
            historyMetadata = historyMetadata
        )

        if (settings.openNextTabInDesktopMode) {
            activity.handleRequestDesktopMode(tabId)
        }

        activity.openToBrowser(BrowserDirection.FromHome)
    }

    override fun handleHistoryShowAllClicked() {
        dismissSearchDialogIfDisplayed()
        navController.navigate(
            HomeFragmentDirections.actionGlobalHistoryFragment()
        )
    }

    override fun handleToggleHistoryMetadataGroupExpanded(historyMetadataGroup: HistoryMetadataGroup) {
        homeFragmentStore.dispatch(
            HomeFragmentAction.HistoryMetadataExpanded(
                historyMetadataGroup
            )
        )
    }

    @VisibleForTesting(otherwise = PRIVATE)
    fun dismissSearchDialogIfDisplayed() {
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }
}
