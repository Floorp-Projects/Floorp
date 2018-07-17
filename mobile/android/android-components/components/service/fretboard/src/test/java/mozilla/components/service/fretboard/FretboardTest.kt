/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
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
        `when`(experimentSource.getExperiments(ArgumentMatchers.anyList())).thenReturn(listOf(Experiment("id")))
        val experimentStorage = mock(ExperimentStorage::class.java)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(listOf())
        verify(experimentStorage).save(listOf(Experiment("id")))
    }

    @Test
    fun testUpdateExperimentsFromStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        `when`(experimentSource.getExperiments(ArgumentMatchers.anyList())).thenReturn(listOf(Experiment("id")))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(listOf(Experiment("id0")))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(listOf(Experiment("id0")))
        verify(experimentStorage).save(listOf(Experiment("id")))
    }
}