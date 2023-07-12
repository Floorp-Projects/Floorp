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
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentState

/**
 * A [Middleware] for initiating navigation events based on [HistoryFragmentAction]s that are
 * dispatched to the [HistoryFragmentStore].
 *
 * @property navController [NavController] for handling navigation events
 * @property openToBrowser Callback to open history items in a browser window.
 */
class HistoryNavigationMiddleware(
    private val navController: NavController,
    private val openToBrowser: (item: History.Regular) -> Unit,
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
                else -> Unit
            }
        }
    }
}
