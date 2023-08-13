/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

/**
 * A [Middleware] for initiating navigation events based on [HistoryFragmentAction]s that are
 * dispatched to the [HistoryFragmentStore].
 *
 * @property navController [NavController] for handling navigation events
 * @property openToBrowser Callback to open history items in a browser window.
 * @property onBackPressed Callback to handle back press actions.
 * @property scope [CoroutineScope] used to launch coroutines.
 */
class HistoryNavigationMiddleware(
    private val navController: NavController,
    private val openToBrowser: (item: History.Regular) -> Unit,
    private val onBackPressed: () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<HistoryFragmentState, HistoryFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HistoryFragmentState, HistoryFragmentAction>,
        next: (HistoryFragmentAction) -> Unit,
        action: HistoryFragmentAction,
    ) {
        // Read the current state before letting the chain process the action, so that clicks are
        // treated correctly in reference to the number of selected items.
        val currentState = context.state
        next(action)
        scope.launch {
            when (action) {
                is HistoryFragmentAction.EnterRecentlyClosed -> {
                    navController.navigate(
                        HistoryFragmentDirections.actionGlobalRecentlyClosed(),
                        NavOptions.Builder().setPopUpTo(R.id.recentlyClosedFragment, true).build(),
                    )
                }
                is HistoryFragmentAction.BackPressed -> {
                    // When editing, we override the back pressed event to update the mode.
                    if (currentState.mode !is HistoryFragmentState.Mode.Editing) {
                        onBackPressed()
                    }
                }
                is HistoryFragmentAction.HistoryItemClicked -> {
                    if (currentState.mode.selectedItems.isEmpty()) {
                        when (val item = action.item) {
                            is History.Regular -> openToBrowser(item)
                            is History.Group -> {
                                navController.navigate(
                                    HistoryFragmentDirections.actionGlobalHistoryMetadataGroup(
                                        title = item.title,
                                        historyMetadataItems = item.items.toTypedArray(),
                                    ),
                                    NavOptions.Builder()
                                        .setPopUpTo(R.id.historyMetadataGroupFragment, true)
                                        .build(),
                                )
                            }
                            else -> Unit
                        }
                    }
                }
                is HistoryFragmentAction.SearchClicked -> {
                    navController.navigateSafe(
                        R.id.historyFragment,
                        HistoryFragmentDirections.actionGlobalSearchDialog(null),
                    )
                }
                else -> Unit
            }
        }
    }
}
