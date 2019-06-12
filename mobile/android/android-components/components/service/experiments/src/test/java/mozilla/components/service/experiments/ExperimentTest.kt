/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ExperimentTest {
    private var currentTime = System.currentTimeMillis() / 1000

    @Test
    fun `test equals()`() {
        val experiment = createDefaultExperiment(
            id = "id",
            description = "description",
            buckets = Experiment.Buckets(0, 100),
            branches = emptyList(),
            match = createDefaultMatcher(),
            lastModified = currentTime,
            schemaModified = null)

        assertTrue(experiment == experiment)
        assertFalse(experiment.equals(null))
        assertFalse(experiment.equals(3))

        val secondExperiment = createDefaultExperiment(
            id = "id",
            description = "description2",
            match = createDefaultMatcher(appId = "eng"),
            lastModified = null,
            schemaModified = null
        )
        assertTrue(secondExperiment == experiment)
    }

    @Test
    fun `test hashCode()`() {
        val experiment = createDefaultExperiment(id = "id")
        assertEquals(experiment.id.hashCode(), experiment.hashCode())
    }
}
