package org.mozilla.fenix.components.metrics

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * A middleware that will map incoming actions to relevant events for [metrics].
 */
class MetricsMiddleware(
    private val metrics: MetricController,
) : Middleware<AppState, AppAction> {
    override fun invoke(
        context: MiddlewareContext<AppState, AppAction>,
        next: (AppAction) -> Unit,
        action: AppAction,
    ) {
        handleAction(action)
        next(action)
    }

    private fun handleAction(action: AppAction) = when (action) {
        is AppAction.ResumedMetricsAction -> {
            metrics.track(Event.GrowthData.SetAsDefault)
            metrics.track(Event.GrowthData.FirstAppOpenForDay)
            metrics.track(Event.GrowthData.FirstWeekSeriesActivity)
        }
        else -> Unit
    }
}
