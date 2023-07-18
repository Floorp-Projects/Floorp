/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import android.content.Context
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger
import mozilla.telemetry.glean.internal.TimerId
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics

private const val PROGRESS_COMPLETE = 100

/**
 * [Middleware] to record telemetry in response to [BrowserAction]s.
 *
 * @property settings reference to the application [Settings].
 * @property metrics [MetricController] to pass events that have been mapped from actions
 * @property nimbusSearchEngine The Nimbus search engine.
 * @property searchState Map that stores the [TabSessionState.id] & [TimerId].
 * @property timerId The [TimerId] for the [Metrics.searchPageLoadTime].
 */
class TelemetryMiddleware(
    private val context: Context,
    private val settings: Settings,
    private val metrics: MetricController,
    private val crashReporting: CrashReporting? = null,
    private val nimbusSearchEngine: String = FxNimbus.features.searchExtraParams.value().searchEngine,
    private val searchState: MutableMap<String, TimerId> = mutableMapOf(),
    private val timerId: TimerId = Metrics.searchPageLoadTime.start(),
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
            is ContentAction.UpdateIsSearchAction -> {
                if (action.isSearch && nimbusSearchEngine.isNotEmpty() &&
                    (action.searchEngineName == nimbusSearchEngine)
                ) {
                    searchState[action.sessionId] = timerId
                }
            }
            is ContentAction.UpdateLoadingStateAction -> {
                context.state.findTab(action.sessionId)?.let { tab ->
                    val hasFinishedLoading = tab.content.loading && !action.loading

                    // Record UriOpened event when a non-private page finishes loading
                    if (hasFinishedLoading) {
                        Events.normalAndPrivateUriCount.add()

                        val progressCompleted = tab.content.progress == PROGRESS_COMPLETE
                        if (progressCompleted) {
                            searchState[action.sessionId]?.let {
                                Metrics.searchPageLoadTime.stopAndAccumulate(it)
                            }
                        } else {
                            searchState[action.sessionId]?.let {
                                Metrics.searchPageLoadTime.cancel(it)
                            }
                        }

                        searchState.remove(action.sessionId)
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
            is EngineAction.LoadUrlAction -> {
                metrics.track(Event.GrowthData.FirstUriLoadForDay)
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
                settings.openPrivateTabsCount = context.state.privateTabs.count()
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

        // Increment the counter of killed foreground/background tabs
        EngineMetrics.tabKilled.record(
            EngineMetrics.TabKilledExtra(
                foregroundTab = isSelected,
                appForeground = context.components.appStore.state.isForeground,
                hadFormData = tab.content.hasFormData,
            ),
        )
    }
}
