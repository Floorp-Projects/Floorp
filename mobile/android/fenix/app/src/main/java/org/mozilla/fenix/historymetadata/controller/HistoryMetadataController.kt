/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.controller

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.HistoryMetadataStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.library.history.toHistoryMetadata

/**
 * An interface that handles the view manipulation of the history metadata in the Home screen.
 */
interface HistoryMetadataController {

    /**
     * @see [HistoryMetadataInteractor.onHistoryMetadataShowAllClicked]
     */
    fun handleHistoryShowAllClicked()

    /**
     * @see [HistoryMetadataInteractor.onHistoryMetadataGroupClicked]
     */
    fun handleHistoryMetadataGroupClicked(historyMetadataGroup: HistoryMetadataGroup)

    /**
     * @see [HistoryMetadataInteractor.onRemoveGroup]
     */
    fun handleRemoveGroup(searchTerm: String)
}

/**
 * The default implementation of [HistoryMetadataController].
 */
class DefaultHistoryMetadataController(
    private val navController: NavController,
    private val storage: HistoryMetadataStorage,
    private val scope: CoroutineScope
) : HistoryMetadataController {

    override fun handleHistoryShowAllClicked() {
        dismissSearchDialogIfDisplayed()
        navController.navigate(
            HomeFragmentDirections.actionGlobalHistoryFragment()
        )
    }

    override fun handleHistoryMetadataGroupClicked(historyMetadataGroup: HistoryMetadataGroup) {
        navController.navigate(
            HomeFragmentDirections.actionGlobalHistoryMetadataGroup(
                title = historyMetadataGroup.title,
                historyMetadataItems = historyMetadataGroup.historyMetadata
                    .map { it.toHistoryMetadata() }.toTypedArray()
            )
        )
    }

    override fun handleRemoveGroup(searchTerm: String) {
        scope.launch {
            storage.deleteHistoryMetadata(searchTerm)
        }
        navController.navigate(
            BrowserFragmentDirections.actionGlobalHome()
        )
    }

    @VisibleForTesting(otherwise = PRIVATE)
    fun dismissSearchDialogIfDisplayed() {
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }
}
