/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ExperimentTest {

    @Test
    fun testEquals() {
        val experiment = Experiment(
            "id",
            "name",
            "description",
            null,
            null,
            12345,
            null)
        assertTrue(experiment == experiment)
        assertFalse(experiment.equals(null))
        assertFalse(experiment.equals(3))
        val secondExperiment = Experiment(
            "id",
            "name2",
            "description2",
            Experiment.Matcher("eng"),
            Experiment.Bucket(100, 0),
            null,
            null
        )
        assertTrue(secondExperiment == experiment)
    }

    @Test
    fun testHashCode() {
        val experiment = Experiment("id", "name")
        assertEquals(experiment.id.hashCode(), experiment.hashCode())
    }
}