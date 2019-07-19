/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

/**
 * A common interface to be implemented by all the histogram-like metric types
 * supported by the Glean SDK.
 */
interface HistogramBase {
    /**
     * Accumulates the provided samples in the metric.
     *
     * Please note that this assumes that the provided samples are already in the
     * "unit" declared by the instance of the implementing metric type (e.g. if the
     * implementing class is a [TimingDistributionMetricType] and the instance this
     * method was called on is using [TimeUnit.Second], then `samples` are assumed
     * to be in that unit).
     *
     * @param samples the [LongArray] holding the samples to be recorded by the metric.
     */
    fun accumulateSamples(samples: LongArray)
}
