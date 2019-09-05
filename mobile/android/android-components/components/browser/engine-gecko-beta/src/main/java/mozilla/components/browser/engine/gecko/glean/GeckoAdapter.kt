/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.glean

import mozilla.components.browser.engine.gecko.GleanMetrics.GleanGeckoHistogramMapping
import org.mozilla.geckoview.RuntimeTelemetry

/**
 * This implements a [RuntimeTelemetry.Delegate] that dispatches Gecko runtime
 * telemetry to the Glean SDK.
 *
 * Metrics defined in the `metrics.yaml` file in Gecko's mozilla-central repository
 * will be automatically dispatched to the Glean SDK and sent through the requested
 * pings.
 *
 * This can be used, in products collecting data through the Glean SDK, by
 * providing an instance to `GeckoRuntimeSettings.Builder().telemetryDelegate`.
 */
class GeckoAdapter : RuntimeTelemetry.Delegate {
    override fun onTelemetryReceived(metric: RuntimeTelemetry.Metric) {
        // Note that the `GleanGeckoHistogramMapping` is automatically generated at
        // build time by the Glean SDK parsers.
        GleanGeckoHistogramMapping[metric.name]?.accumulateSamples(metric.values)
    }
}
