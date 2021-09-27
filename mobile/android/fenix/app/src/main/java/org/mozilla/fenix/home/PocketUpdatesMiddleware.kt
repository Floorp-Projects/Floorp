/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.pocket.PocketStoriesService

/**
 * [HomeFragmentStore] middleware reacting in response to Pocket related [Action]s.
 */
class PocketUpdatesMiddleware(
    private val coroutineScope: CoroutineScope,
    private val pocketStoriesService: PocketStoriesService
) : Middleware<HomeFragmentState, HomeFragmentAction> {
    override fun invoke(
        context: MiddlewareContext<HomeFragmentState, HomeFragmentAction>,
        next: (HomeFragmentAction) -> Unit,
        action: HomeFragmentAction
    ) {
        next(action)

        // Post process actions
        when (action) {
            is HomeFragmentAction.PocketStoriesShown -> {
                coroutineScope.launch {
                    pocketStoriesService.updateStoriesTimesShown(
                        action.storiesShown.map {
                            it.copy(timesShown = it.timesShown.inc())
                        }
                    )
                }
            }
            else -> {
                // no-op
            }
        }
    }
}
