/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry
import org.mozilla.fenix.utils.Settings

/**
 * [Middleware] to record telemetry in response to [BrowserAction]s.
 *
 * @property settings reference to the application [Settings].
 * @property adsTelemetry reference to [AdsTelemetry] use to record search telemetry.
 * @property metrics reference to the configured [MetricController] to record general page load events.
 */
class TelemetryMiddleware(
    private val settings: Settings,
    private val adsTelemetry: AdsTelemetry,
    private val metrics: MetricController
) : Middleware<BrowserState, BrowserAction> {

    private val logger = Logger("TelemetryMiddleware")

    @VisibleForTesting
    internal val redirectChains = mutableMapOf<String, RedirectChain>()

    /**
     * Utility to collect URLs / load requests in between location changes.
     */
    internal class RedirectChain(internal val root: String) {
        internal val chain = mutableListOf<String>()

        fun add(url: String) {
            chain.add(url)
        }
    }

    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        // Pre process actions
        when (action) {
            is ContentAction.UpdateLoadingStateAction -> {
                context.state.findTab(action.sessionId)?.let { tab ->
                    // Record UriOpened event when a non-private page finishes loading
                    if (tab.content.loading && !action.loading && !tab.content.private) {
                        metrics.track(Event.UriOpened)
                    }
                }
            }
            is ContentAction.UpdateLoadRequestAction -> {
                context.state.findTab(action.sessionId)?.let { tab ->
                    // Collect all load requests in between location changes
                    if (!redirectChains.containsKey(action.sessionId) && action.loadRequest.url != tab.content.url) {
                        redirectChains[action.sessionId] = RedirectChain(tab.content.url)
                    }

                    redirectChains[action.sessionId]?.add(action.loadRequest.url)
                }
            }
            is ContentAction.UpdateUrlAction -> {
                redirectChains[action.sessionId]?.let {
                    // Record ads telemetry providing all redirects
                    try {
                        adsTelemetry.trackAdClickedMetric(it.root, it.chain)
                    } catch (t: Throwable) {
                        logger.info("Failed to record search telemetry", t)
                    } finally {
                        redirectChains.remove(action.sessionId)
                    }
                }
            }
            is DownloadAction.AddDownloadAction -> {
                metrics.track(Event.DownloadAdded)
            }
        }

        next(action)

        // Post process actions
        when (action) {
            is TabListAction.AddTabAction,
            is TabListAction.AddMultipleTabsAction,
            is TabListAction.RemoveTabAction,
            is TabListAction.RemoveAllNormalTabsAction,
            is TabListAction.RemoveAllTabsAction,
            is TabListAction.RestoreAction -> {
                // Update/Persist tabs count whenever it changes
                settings.openTabsCount = context.state.normalTabs.count()
                if (context.state.normalTabs.count() > 0) {
                    metrics.track(Event.HaveOpenTabs)
                } else {
                    metrics.track(Event.HaveNoOpenTabs)
                }
            }
        }
    }
}
