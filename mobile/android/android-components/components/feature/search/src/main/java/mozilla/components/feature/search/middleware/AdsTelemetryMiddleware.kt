/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.middleware

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger

/**
 * [BrowserStore] middleware to be used alongside with [AdsTelemetry] to check when an ad shown
 * in search results is clicked.
 */
class AdsTelemetryMiddleware(
    private val adsTelemetry: AdsTelemetry,
) : Middleware<BrowserState, BrowserAction> {
    @VisibleForTesting
    internal val redirectChain = mutableMapOf<String, RedirectChain>()
    private val logger = Logger("AdsTelemetryMiddleware")

    @Suppress("TooGenericExceptionCaught")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is ContentAction.UpdateLoadRequestAction -> {
                context.state.findTab(action.sessionId)?.let { tab ->
                    // Collect all load requests in between location changes
                    if (!redirectChain.containsKey(action.sessionId) && action.loadRequest.url != tab.content.url) {
                        redirectChain[action.sessionId] = RedirectChain(tab.content.url)
                    }

                    redirectChain[action.sessionId]?.add(action.loadRequest.url)
                }
            }
            is ContentAction.UpdateUrlAction -> {
                redirectChain[action.sessionId]?.let {
                    // Record ads telemetry providing all redirects
                    try {
                        adsTelemetry.checkIfAddWasClicked(it.root, it.chain)
                    } catch (t: Throwable) {
                        logger.info("Failed to record search telemetry", t)
                    } finally {
                        redirectChain.remove(action.sessionId)
                    }
                }
            }
            else -> {
                // no-op
            }
        }

        next(action)
    }
}

/**
 * Utility to collect URLs / load requests in between location changes.
 */
@VisibleForTesting
internal class RedirectChain(val root: String) {
    val chain = mutableListOf<String>()

    fun add(url: String) {
        chain.add(url)
    }
}
