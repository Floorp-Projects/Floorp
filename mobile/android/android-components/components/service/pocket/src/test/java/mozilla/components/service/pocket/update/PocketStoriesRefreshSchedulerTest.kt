/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.update.RefreshPocketWorker.Companion.REFRESH_WORK_TAG
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketStoriesRefreshSchedulerTest {
    @Test
    fun `GIVEN a PocketStoriesRefreshScheduler THEN its visibility is internal`() {
        assertClassVisibility(PocketStoriesRefreshScheduler::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a PocketStoriesRefreshScheduler WHEN schedulePeriodicRefreshes THEN a RefreshPocketWorker is created and enqueued`() {
        val client: HttpURLConnectionClient = mock()
        val scheduler = spy(
            PocketStoriesRefreshScheduler(
                PocketStoriesConfig(
                    client,
                    Frequency(1, TimeUnit.HOURS),
                ),
            ),
        )
        val workManager = mock<WorkManager>()
        val worker = mock<PeriodicWorkRequest>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())
        doReturn(worker).`when`(scheduler).createPeriodicWorkerRequest(any())

        scheduler.schedulePeriodicRefreshes(testContext)

        verify(workManager).enqueueUniquePeriodicWork(REFRESH_WORK_TAG, ExistingPeriodicWorkPolicy.KEEP, worker)
    }

    @Test
    fun `GIVEN a PocketStoriesRefreshScheduler WHEN stopPeriodicRefreshes THEN it should cancel all unfinished work`() {
        val scheduler = spy(PocketStoriesRefreshScheduler(mock()))
        val workManager = mock<WorkManager>()
        doReturn(workManager).`when`(scheduler).getWorkManager(any())

        scheduler.stopPeriodicRefreshes(testContext)

        verify(workManager).cancelAllWorkByTag(REFRESH_WORK_TAG)
        verify(workManager, Mockito.never()).cancelAllWork()
    }

    @Test
    fun `GIVEN a PocketStoriesRefreshScheduler WHEN createPeriodicWorkerRequest THEN a properly configured PeriodicWorkRequest is returned`() {
        val scheduler = spy(PocketStoriesRefreshScheduler(mock()))

        val result = scheduler.createPeriodicWorkerRequest(
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
        assertTrue(result.tags.contains(REFRESH_WORK_TAG))
    }

    @Test
    fun `GIVEN PocketStoriesRefreshScheduler THEN Worker constraints should be to have Internet`() {
        val scheduler = PocketStoriesRefreshScheduler(mock())

        val result = scheduler.getWorkerConstrains()

        assertFalse(result.requiresBatteryNotLow())
        assertFalse(result.requiresCharging())
        assertFalse(result.hasContentUriTriggers())
        assertFalse(result.requiresStorageNotLow())
        assertFalse(result.requiresDeviceIdle())
        assertTrue(result.requiredNetworkType == NetworkType.CONNECTED)
    }
}
