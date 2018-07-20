/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FretboardTest {
    @Test
    fun testLoadExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()
        verify(experimentStorage).retrieve()
    }

    @Test
    fun testUpdateExperimentsStorageNotLoaded() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
    }

    @Test
    fun testUpdateExperimentsEmptyStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        val result = ExperimentsSnapshot(listOf(), null)
        `when`(experimentSource.getExperiments(result)).thenReturn(ExperimentsSnapshot(listOf(Experiment("id")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(result)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(result)
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id")), null))
    }

    @Test
    fun testUpdateExperimentsFromStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        `when`(experimentSource.getExperiments(ExperimentsSnapshot(listOf(Experiment("id0")), null))).thenReturn(ExperimentsSnapshot(listOf(Experiment("id")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(Experiment("id0")), null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(ExperimentsSnapshot(listOf(Experiment("id0")), null))
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id")), null))
    }

    @Test
    fun testExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id"),
            Experiment("second-id")
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        var returnedExperiments = fretboard.experiments
        assertEquals(0, returnedExperiments.size)
        fretboard.loadExperiments()
        returnedExperiments = fretboard.experiments
        assertEquals(2, returnedExperiments.size)
        assertTrue(returnedExperiments.contains(experiments[0]))
        assertTrue(returnedExperiments.contains(experiments[1]))
    }

    @Test
    fun testExperimentsNoExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf<Experiment>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        val returnedExperiments = fretboard.experiments
        assertEquals(0, returnedExperiments.size)
    }
}