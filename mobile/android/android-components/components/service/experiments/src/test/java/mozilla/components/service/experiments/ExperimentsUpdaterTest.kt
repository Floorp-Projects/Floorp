/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkerParameters
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.spy
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class ExperimentsUpdaterTest {
    private var context: Context = ApplicationProvider.getApplicationContext()
    private lateinit var configuration: Configuration
    private lateinit var experiments: ExperimentsInternalAPI
    private lateinit var experimentStorage: FlatFileExperimentStorage
    private lateinit var experimentSource: KintoExperimentSource
    private lateinit var experimentsUpdater: ExperimentsUpdater
    private lateinit var workRequest: PeriodicWorkRequest

    private fun getDefaultExperimentStorageMock(): FlatFileExperimentStorage {
        val storage: FlatFileExperimentStorage = mock()
        `when`(storage.retrieve()).thenReturn(ExperimentsSnapshot(emptyList(), null))
        return storage
    }

    private fun getDefaultExperimentSourceMock(): KintoExperimentSource {
        val source: KintoExperimentSource = mock()
        `when`(source.getExperiments(mozilla.components.support.test.any())).thenReturn(ExperimentsSnapshot(emptyList(), null))
        return source
    }

    private fun resetExperiments(
        source: KintoExperimentSource = getDefaultExperimentSourceMock(),
        storage: FlatFileExperimentStorage = getDefaultExperimentStorageMock(),
        valuesProvider: ValuesProvider = ValuesProvider()
    ) {
        // Initialize WorkManager (early) for instrumentation tests.
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        // Setting the endpoint to a non-existent one to prevent actual experiments from being
        // downloaded to tests.
        configuration = Configuration(kintoEndpoint = "https://example.invalid")
        experiments = spy(ExperimentsInternalAPI())
        experiments.valuesProvider = valuesProvider

        experimentStorage = storage
        `when`(experiments.getExperimentsStorage(context)).thenReturn(storage)

        experimentsUpdater = spy(ExperimentsUpdater(context, experiments))
        `when`(experiments.getExperimentsUpdater(context)).thenReturn(experimentsUpdater)
        experimentSource = source
        `when`(experimentsUpdater.getExperimentSource(configuration)).thenReturn(experimentSource)

        workRequest = experimentsUpdater.getWorkRequest()
        `when`(experimentsUpdater.getWorkRequest()).thenReturn(workRequest)
    }

    @Before
    fun setUp() {
        resetExperiments()
    }

    @Test
    fun `ExperimentsUpdater initialize schedules ExperimentsUpdateWorker in WorkManager`() {
        assertFalse("ExperimentsUpdateWorker should not be scheduled yet",
            isWorkScheduled(ExperimentsUpdater.TAG))
        experimentsUpdater.initialize(configuration)
        assertTrue("initialize must schedule ExperimentsUpdateWorker",
            isWorkScheduled(ExperimentsUpdater.TAG))
        val workInfo = getWorkInfoByTag(ExperimentsUpdater.TAG)
        assertNotNull("workInfo must not be null", workInfo)
        assertEquals("workInfo id must match request id", workRequest.id, workInfo?.id)
    }

    @Test
    fun `ExperimentsUpdateWorker doWork returns SUCCESS`() {
        experiments.initialize(context, configuration)
        experimentsUpdater.initialize(configuration)
        // Replace the updater in the singleton with the one built for the test
        Experiments.updater = experimentsUpdater
        val workerParameters: WorkerParameters = mock()
        val worker = ExperimentsUpdaterWorker(context, workerParameters)
        val result = worker.doWork()
        assertTrue(result.toString().contains("Success"))
    }

    @Test
    fun `ExperimentsUpdateWorker doWork returns FAILURE when download fails`() {
        experiments.initialize(context, configuration)
        // Mock the updater in order to be able to return false from updateExperiments()
        experimentsUpdater = mock()
        `when`(experimentsUpdater.updateExperiments()).thenReturn(false)

        // Replace the updater in the singleton with the one built for the test
        Experiments.updater = experimentsUpdater
        val workerParameters: WorkerParameters = mock()
        val worker = ExperimentsUpdaterWorker(context, workerParameters)
        val result = worker.doWork()
        assertTrue(result.toString().contains("Failure"))
    }

    @Test
    fun `getWorkRequest returns request with correct parameters`() {
        val workRequest = experimentsUpdater.getWorkRequest()
        assertTrue("work request must be properly tagged",
            workRequest.tags.contains(ExperimentsUpdater.TAG))

        assertTrue("work request must be periodic", workRequest.workSpec.isPeriodic)

        // Work request converts the initial `HOURS` delay to milliseconds, so we must do the same
        // when we check this.
        assertEquals("work request must have correct interval duration",
            TimeUnit.HOURS.toMillis(ExperimentsUpdater.UPDATE_INTERVAL_HOURS),
            workRequest.workSpec.intervalDuration)

        val constraints = workRequest.workSpec.constraints
        assertEquals("work request constraints must have correct network type",
            NetworkType.CONNECTED, constraints.requiredNetworkType)
    }
}
