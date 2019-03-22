/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.service.glean.EventMetricType
import mozilla.components.service.glean.Glean
import mozilla.components.support.utils.Browsers
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.debug.GleanMetrics.Metrics
import org.mozilla.fenix.debug.GleanMetrics.Events

private class EventWrapper<T : Enum<T>>(
    private val event: EventMetricType<T>,
    private val keyMapper: ((String) -> T)? = null
) {
    fun track(event: Event) {
        val extras = if (keyMapper != null) {
            event.extras?.mapKeys { keyMapper.invoke(it.key) }
        } else {
            null
        }

        this.event.record(extras)
    }
}

private val Event.wrapper
    get() = when (this) {
        is Event.OpenedApp -> EventWrapper(Events.appOpened) { Events.appOpenedKeys.valueOf(it) }
        is Event.SearchBarTapped -> EventWrapper(Events.searchBarTapped) { Events.searchBarTappedKeys.valueOf(it) }
        is Event.EnteredUrl -> EventWrapper(Events.enteredUrl) { Events.enteredUrlKeys.valueOf(it) }
        is Event.PerformedSearch -> EventWrapper(Events.performedSearch) { Events.performedSearchKeys.valueOf(it) }
        else -> null
    }

class GleanMetricsService(private val context: Context) : MetricsService {
    override fun start() {
        Glean.initialize(context)
        Glean.setUploadEnabled(IsGleanEnabled)

        Metrics.apply {
            defaultBrowser.set(Browsers.all(context).isDefaultBrowser)
        }
    }

    override fun track(event: Event) {
        event.wrapper?.track(event)
    }

    override fun shouldTrack(event: Event): Boolean {
        return Settings.getInstance(context).isTelemetryEnabled && event.wrapper != null
    }

    companion object {
        private const val IsGleanEnabled = BuildConfig.TELEMETRY
    }
}
