/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.BackoffPolicy
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.update.DeleteSpocsProfileWorker.Companion.DELETE_SPOCS_PROFILE_WORK_TAG
import mozilla.components.service.pocket.update.RefreshSpocsWorker.Companion.REFRESH_SPOCS_WORK_TAG
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class SpocsRefreshSchedulerTest {
    @Test
    fun `GIVEN a spocs refresh scheduler THEN its visibility is internal`() {
        assertClassVisibility(SpocsRefreshScheduler::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN scheduling stories refresh THEN a RefreshPocketWorker is created and enqueued`() {
        val client: HttpURLConnectionClient = mock()
        val scheduler = spy(
            SpocsRefreshScheduler(
                PocketStoriesConfig(
                    client,
                    Frequency(1, TimeUnit.HOURS),
                ),
            ),
        )
        val workManager = mock<WorkManager>()
        val worker = mock<PeriodicWorkRequest>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())
        doReturn(worker).`when`(scheduler).createPeriodicRefreshWorkerRequest(any())

        scheduler.schedulePeriodicRefreshes(testContext)

        verify(workManager).enqueueUniquePeriodicWork(REFRESH_SPOCS_WORK_TAG, ExistingPeriodicWorkPolicy.KEEP, worker)
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN stopping stories refresh THEN it should cancel all unfinished work`() {
        val scheduler = spy(SpocsRefreshScheduler(mock()))
        val workManager = mock<WorkManager>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())

        scheduler.stopPeriodicRefreshes(testContext)

        verify(workManager).cancelAllWorkByTag(REFRESH_SPOCS_WORK_TAG)
        verify(workManager, Mockito.never()).cancelAllWork()
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN scheduling profile deletion THEN a RefreshPocketWorker is created and enqueued`() {
        val client: HttpURLConnectionClient = mock()
        val scheduler = spy(
            SpocsRefreshScheduler(
                PocketStoriesConfig(
                    client,
                    Frequency(1, TimeUnit.HOURS),
                ),
            ),
        )
        val workManager = mock<WorkManager>()
        val worker = mock<OneTimeWorkRequest>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())
        doReturn(worker).`when`(scheduler).createOneTimeProfileDeletionWorkerRequest()

        scheduler.scheduleProfileDeletion(testContext)

        verify(workManager).enqueueUniqueWork(DELETE_SPOCS_PROFILE_WORK_TAG, ExistingWorkPolicy.KEEP, worker)
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN cancelling profile deletion THEN it should cancel all unfinished work`() {
        val scheduler = spy(SpocsRefreshScheduler(mock()))
        val workManager = mock<WorkManager>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())

        scheduler.stopProfileDeletion(testContext)

        verify(workManager).cancelAllWorkByTag(DELETE_SPOCS_PROFILE_WORK_TAG)
        verify(workManager, never()).cancelAllWork()
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN creating a periodic worker THEN a properly configured PeriodicWorkRequest is returned`() {
        val scheduler = spy(SpocsRefreshScheduler(mock()))

        val result = scheduler.createPeriodicRefreshWorkerRequest(
            Frequency(1, TimeUnit.HOURS),
        )

        verify(scheduler).getWorkerConstrains()
        assertTrue(result.workSpec.intervalDuration == TimeUnit.HOURS.toMillis(1))
        assertFalse(result.workSpec.constraints.requiresBatteryNotLow())
        assertFalse(result.workSpec.constraints.requiresCharging())
        assertFalse(result.workSpec.constraints.hasContentUriTriggers())
        assertFalse(result.workSpec.constraints.requiresStorageNotLow())
        assertFalse(result.workSpec.constraints.requiresDeviceIdle())
        assertTrue(result.workSpec.constraints.requiredNetworkType == NetworkType.CONNECTED)
        assertTrue(result.tags.contains(REFRESH_SPOCS_WORK_TAG))
    }

    @Test
    fun `GIVEN a spocs refresh scheduler WHEN creating a one time worker THEN a properly configured OneTimeWorkRequest is returned`() {
        val scheduler = spy(SpocsRefreshScheduler(mock()))

        val result = scheduler.createOneTimeProfileDeletionWorkerRequest()

        verify(scheduler).getWorkerConstrains()
        assertEquals(0, result.workSpec.intervalDuration)
        assertEquals(0, result.workSpec.initialDelay)
        assertEquals(BackoffPolicy.EXPONENTIAL, result.workSpec.backoffPolicy)
        assertFalse(result.workSpec.constraints.requiresBatteryNotLow())
        assertFalse(result.workSpec.constraints.requiresCharging())
        assertFalse(result.workSpec.constraints.hasContentUriTriggers())
        assertFalse(result.workSpec.constraints.requiresStorageNotLow())
        assertFalse(result.workSpec.constraints.requiresDeviceIdle())
        assertTrue(result.workSpec.constraints.requiredNetworkType == NetworkType.CONNECTED)
        assertTrue(result.tags.contains(DELETE_SPOCS_PROFILE_WORK_TAG))
    }

    @Test
    fun `GIVEN a spocs refresh scheduler THEN Worker constraints should be to have Internet`() {
        val scheduler = SpocsRefreshScheduler(mock())

        val result = scheduler.getWorkerConstrains()

        assertFalse(result.requiresBatteryNotLow())
        assertFalse(result.requiresCharging())
        assertFalse(result.hasContentUriTriggers())
        assertFalse(result.requiresStorageNotLow())
        assertFalse(result.requiresDeviceIdle())
        assertTrue(result.requiredNetworkType == NetworkType.CONNECTED)
    }
}
