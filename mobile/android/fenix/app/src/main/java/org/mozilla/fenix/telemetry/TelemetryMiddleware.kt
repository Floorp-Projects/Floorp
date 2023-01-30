/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.android.Clock
import mozilla.components.support.base.log.logger.Logger
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics

/**
 * [Middleware] to record telemetry in response to [BrowserAction]s.
 *
 * @property settings reference to the application [Settings].
 * @property metrics [MetricController] to pass events that have been mapped from actions
 */
class TelemetryMiddleware(
    private val settings: Settings,
    private val metrics: MetricController,
    private val crashReporting: CrashReporting? = null,
) : Middleware<BrowserState, BrowserAction> {

    private val logger = Logger("TelemetryMiddleware")

    @Suppress("TooGenericExceptionCaught", "ComplexMethod", "NestedBlockDepth")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        // Pre process actions
        when (action) {
            is ContentAction.UpdateLoadingStateAction -> {
                context.state.findTab(action.sessionId)?.let { tab ->
                    // Record UriOpened event when a non-private page finishes loading
                    if (tab.content.loading && !action.loading) {
                        Events.normalAndPrivateUriCount.add()
                    }
                }
            }
            is DownloadAction.AddDownloadAction -> { /* NOOP */ }
            is EngineAction.KillEngineSessionAction -> {
                val tab = context.state.findTabOrCustomTab(action.tabId)
                onEngineSessionKilled(context.state, tab)
            }
            is ContentAction.CheckForFormDataExceptionAction -> {
                Events.formDataFailure.record(NoExtras())
                if (Config.channel.isNightlyOrDebug) {
                    crashReporting?.submitCaughtException(action.throwable)
                }
                return
            }
            else -> {
                // no-op
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
            is TabListAction.RestoreAction,
            -> {
                // Update/Persist tabs count whenever it changes
                settings.openTabsCount = context.state.normalTabs.count()
                if (context.state.normalTabs.isNotEmpty()) {
                    Metrics.hasOpenTabs.set(true)
                } else {
                    Metrics.hasOpenTabs.set(false)
                }
            }
            else -> {
                // no-op
            }
        }
    }

    /**
     * Collecting some engine-specific (GeckoView) telemetry.
     * https://github.com/mozilla-mobile/android-components/issues/9366
     */
    private fun onEngineSessionKilled(state: BrowserState, tab: SessionState?) {
        if (tab == null) {
            logger.debug("Could not find tab for killed engine session")
            return
        }

        val isSelected = tab.id == state.selectedTabId
        val age = tab.engineState.age()

        // Increment the counter of killed foreground/background tabs
        val tabKillLabel = if (isSelected) { "foreground" } else { "background" }
        EngineMetrics.kills[tabKillLabel].add()

        // Record the age of the engine session of the killed foreground/background tab.
        if (isSelected && age != null) {
            EngineMetrics.killForegroundAge.accumulateSamples(listOf(age))
        } else if (age != null) {
            EngineMetrics.killBackgroundAge.accumulateSamples(listOf(age))
        }
    }
}

@Suppress("MagicNumber")
private fun EngineState.age(): Long? {
    val timestamp = (timestamp ?: return null)
    val now = Clock.elapsedRealtime()
    return (now - timestamp)
}
