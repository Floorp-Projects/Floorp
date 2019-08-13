/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

/**
 * A common interface to be implemented by all the histogram-like metric types
 * supported by the Glean SDK.
 */
interface HistogramMetricBase {
    /**
     * Accumulates the provided samples in the metric.
     *
     * @param samples the [LongArray] holding the samples to be recorded by the metric.
     */
    fun accumulateSamples(samples: LongArray)
}
