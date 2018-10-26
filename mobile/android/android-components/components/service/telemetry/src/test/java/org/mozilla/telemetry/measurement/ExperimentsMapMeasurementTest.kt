/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ExperimentsMapMeasurementTest {
    @Test
    fun `measurement returns map with "control" and "branch" values`() {
        val experiments = mapOf(
            "use-gecko" to true,
            "use-gecko-nightly" to false,
            "use-homescreen-tips" to true,
            "use-homescreen-tips-nightly" to false
        )

        val measurement = ExperimentsMapMeasurement()
        measurement.setExperiments(experiments)

        val value = measurement.flush()

        assertEquals(4, value.length())
        assertEquals("branch", value.get("use-gecko"))
        assertEquals("control", value.get("use-gecko-nightly"))
        assertEquals("branch", value.get("use-homescreen-tips"))
        assertEquals("control", value.get("use-homescreen-tips-nightly"))
    }

    @Test
    fun `measurement returns empty object by default`() {
        val value = ExperimentsMapMeasurement().flush()
        assertEquals(0, value.length())
    }
}
