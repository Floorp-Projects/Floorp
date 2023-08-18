/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

/**
 * Middleware that handles navigation events for the review quality check feature.
 *
 * @property openLink Callback used to open an url.
 * @property scope [CoroutineScope] used to launch coroutines.
 */
class ReviewQualityCheckNavigationMiddleware(
    private val openLink: (String, Boolean) -> Unit,
    private val scope: CoroutineScope,
) : Middleware<ReviewQualityCheckState, ReviewQualityCheckAction> {

    override fun invoke(
        context: MiddlewareContext<ReviewQualityCheckState, ReviewQualityCheckAction>,
        next: (ReviewQualityCheckAction) -> Unit,
        action: ReviewQualityCheckAction,
    ) {
        next(action)
        when (action) {
            is ReviewQualityCheckAction.NavigationMiddlewareAction -> processAction(action)
            else -> {
                // no-op
            }
        }
    }

    private fun processAction(
        action: ReviewQualityCheckAction.NavigationMiddlewareAction,
    ) = scope.launch {
        when (action) {
            is ReviewQualityCheckAction.OpenLink -> {
                when (action.link) {
                    is ReviewQualityCheckState.LinkType.ExternalLink -> openLink(action.link.url, true)
                    is ReviewQualityCheckState.LinkType.AnalyzeLink -> openLink(action.link.url, false)
                }
            }
        }
    }
}
