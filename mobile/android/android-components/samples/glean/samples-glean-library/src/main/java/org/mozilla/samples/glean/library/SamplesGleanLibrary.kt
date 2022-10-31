/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.glean.library

import mozilla.components.service.glean.Glean
import mozilla.samples.glean.library.GleanMetrics.SampleMetrics

/**
 * These are just simple functions to test calling the Glean API
 * from a third-party library.
 */
object SamplesGleanLibrary {
    /**
     * Record to a metric defined in *this* library's metrics.yaml file.
     */
    fun recordMetric() {
        SampleMetrics.test.add()
    }

    /**
     * Notate an active experiment.
     */
    fun recordExperiment() {
        Glean.setExperimentActive(
            "third_party_library",
            "enabled",
        )
    }
}
