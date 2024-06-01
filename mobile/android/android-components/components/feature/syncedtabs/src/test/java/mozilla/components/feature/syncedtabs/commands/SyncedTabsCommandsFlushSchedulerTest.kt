/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.BackoffPolicy
import androidx.work.Configuration
import androidx.work.NetworkType
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.WorkRequest
import androidx.work.await
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.time.Duration.Companion.minutes
import kotlin.time.Duration.Companion.seconds

@RunWith(AndroidJUnit4::class)
class SyncedTabsCommandsFlushSchedulerTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var workManager: WorkManager

    @Before
    fun setUp() {
        GlobalSyncedTabsCommandsProvider.initialize(lazy { mock() })

        WorkManagerTestInitHelper.initializeTestWorkManager(
            testContext,
            Configuration.Builder().build(),
        )
        workManager = WorkManager.getInstance(testContext)
    }

    @After
    fun tearDown() {
        workManager.cancelAllWork()
    }

    @Test
    fun `GIVEN a scheduler WHEN a flush is requested THEN work should be enqueued`() = runTestOnMain {
        val scheduler = SyncedTabsCommandsFlushScheduler(testContext)

        scheduler.requestFlush()

        val workInfos = workManager.getWorkInfosForUniqueWork(SyncedTabsCommandsFlushWorker.WORK_TAG).await()
        assertEquals(1, workInfos.size)
        assertEquals(WorkInfo.State.ENQUEUED, workInfos.first().state)
    }

    @Test
    fun `GIVEN a scheduler WHEN a flush is cancelled THEN work should be cancelled`() = runTestOnMain {
        val scheduler = SyncedTabsCommandsFlushScheduler(testContext)
        scheduler.requestFlush()

        scheduler.cancelFlush()

        val workInfos = workManager.getWorkInfosForUniqueWork(SyncedTabsCommandsFlushWorker.WORK_TAG).await()
        assertEquals(1, workInfos.size)
        assertEquals(WorkInfo.State.CANCELLED, workInfos.first().state)
    }

    @Test
    fun `GIVEN a scheduler without a flush delay WHEN a flush work request is created THEN the request should be configured`() = runTestOnMain {
        val scheduler = SyncedTabsCommandsFlushScheduler(testContext)
        val workRequest = scheduler.createFlushWorkRequest()

        assertFalse(workRequest.workSpec.isPeriodic)
        assertTrue(workRequest.tags.contains(SyncedTabsCommandsFlushWorker.WORK_TAG))
        assertEquals(NetworkType.CONNECTED, workRequest.workSpec.constraints.requiredNetworkType)
        assertEquals(BackoffPolicy.EXPONENTIAL, workRequest.workSpec.backoffPolicy)
        assertEquals(0, workRequest.workSpec.initialDelay)
        assertEquals(WorkRequest.MIN_BACKOFF_MILLIS, workRequest.workSpec.backoffDelayDuration)
    }

    @Test
    fun `GIVEN a scheduler with a flush delay WHEN a flush work request is created THEN the request should be configured`() = runTestOnMain {
        val scheduler = SyncedTabsCommandsFlushScheduler(
            testContext,
            flushDelay = 5.minutes,
        )
        val workRequest = scheduler.createFlushWorkRequest()

        assertEquals(5.minutes.inWholeMilliseconds, workRequest.workSpec.initialDelay)
    }

    @Test
    fun `GIVEN a scheduler with a retry delay WHEN a flush work request is created THEN the request should be configured`() = runTestOnMain {
        val scheduler = SyncedTabsCommandsFlushScheduler(
            testContext,
            retryDelay = 45.seconds,
        )
        val workRequest = scheduler.createFlushWorkRequest()

        assertEquals(45.seconds.inWholeMilliseconds, workRequest.workSpec.backoffDelayDuration)
    }
}
