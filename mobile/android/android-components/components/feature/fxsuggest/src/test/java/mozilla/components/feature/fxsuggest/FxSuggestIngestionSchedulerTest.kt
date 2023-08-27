/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.Configuration
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.await
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class FxSuggestIngestionSchedulerTest {
    private lateinit var storage: FxSuggestStorage
    private lateinit var workManager: WorkManager

    @Before
    fun setUp() {
        storage = mock()
        GlobalFxSuggestDependencyProvider.storage = storage

        WorkManagerTestInitHelper.initializeTestWorkManager(
            testContext,
            Configuration.Builder().build(),
        )
        workManager = WorkManager.getInstance(testContext)
    }

    @After
    fun tearDown() {
        workManager.cancelAllWork()
        GlobalFxSuggestDependencyProvider.storage = null
    }

    @Test
    fun startPeriodicIngestion() = runTest {
        val scheduler = FxSuggestIngestionScheduler(testContext)

        scheduler.startPeriodicIngestion()

        val workInfos = workManager.getWorkInfosForUniqueWork(FxSuggestIngestionWorker.WORK_TAG).await()
        assertEquals(1, workInfos.size)
        assertEquals(WorkInfo.State.ENQUEUED, workInfos.first().state)
    }

    @Test
    fun stopPeriodicIngestion() = runTest {
        val scheduler = FxSuggestIngestionScheduler(testContext)
        scheduler.startPeriodicIngestion()

        scheduler.stopPeriodicIngestion()

        val workInfos = workManager.getWorkInfosForUniqueWork(FxSuggestIngestionWorker.WORK_TAG).await()
        assertEquals(1, workInfos.size)
        assertEquals(WorkInfo.State.CANCELLED, workInfos.first().state)
    }

    @Test
    fun createPeriodicIngestionWorkerRequest() = runTest {
        val scheduler = FxSuggestIngestionScheduler(testContext)

        val workRequest = scheduler.createPeriodicIngestionWorkerRequest()

        assertTrue(workRequest.workSpec.isPeriodic)
        assertTrue(workRequest.tags.contains(FxSuggestIngestionWorker.WORK_TAG))
        assertEquals(1 * 24 * 60 * 60 * 1000, workRequest.workSpec.intervalDuration)
        assertEquals(scheduler.getWorkerConstrains(), workRequest.workSpec.constraints)
    }
}
