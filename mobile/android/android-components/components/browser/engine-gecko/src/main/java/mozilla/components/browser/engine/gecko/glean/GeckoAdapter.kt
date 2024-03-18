/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.glean

import mozilla.components.browser.engine.gecko.GleanMetrics.GleanGeckoMetricsMapping
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
    // Note that the `GleanGeckoMetricsMapping` is automatically generated at
    // build time by the Glean SDK parsers.

    override fun onHistogram(metric: RuntimeTelemetry.Histogram) {
        if (metric.isCategorical) {
            // Gecko categorical histograms are a bit special: their value indicates
            // the index of the label they want to accumulate 1 unit to. Moreover,
            // Gecko batches them up before sending: each value in `metric.value` is
            // the index of a potentially different label.
            GleanGeckoMetricsMapping.getCategoricalMetric(metric.name)?.let { categorical ->
                metric.value.forEach { labelIndex -> categorical[labelIndex.toInt()].add(1) }
            }
        } else {
            GleanGeckoMetricsMapping.getHistogram(metric.name)?.accumulateSamples(metric.value.toList())
        }
    }

    override fun onBooleanScalar(metric: RuntimeTelemetry.Metric<Boolean>) {
        GleanGeckoMetricsMapping.getBooleanScalar(metric.name)?.set(metric.value)
    }

    override fun onStringScalar(metric: RuntimeTelemetry.Metric<String>) {
        GleanGeckoMetricsMapping.getStringScalar(metric.name)?.set(metric.value)
    }

    override fun onLongScalar(metric: RuntimeTelemetry.Metric<Long>) {
        GleanGeckoMetricsMapping.getQuantityScalar(metric.name)?.set(metric.value)
    }
}
