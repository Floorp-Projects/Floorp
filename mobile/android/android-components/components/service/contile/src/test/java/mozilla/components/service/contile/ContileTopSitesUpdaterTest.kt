/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.Configuration
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.await
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.contile.ContileTopSitesUpdater.Companion.PERIODIC_WORK_TAG
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContileTopSitesUpdaterTest {

    @Before
    fun setUp() {
        WorkManagerTestInitHelper.initializeTestWorkManager(
            testContext,
            Configuration.Builder().build(),
        )
    }

    @After
    fun tearDown() {
        WorkManager.getInstance(testContext).cancelUniqueWork(PERIODIC_WORK_TAG)
    }

    @Test
    fun `WHEN periodic work is started THEN work is queued`() = runTest {
        val updater = ContileTopSitesUpdater(testContext, provider = mock())
        val workManager = WorkManager.getInstance(testContext)
        var workInfo = workManager.getWorkInfosForUniqueWork(PERIODIC_WORK_TAG).await()

        assertTrue(workInfo.isEmpty())
        assertNull(ContileTopSitesUseCases.provider)

        updater.startPeriodicWork()

        assertNotNull(ContileTopSitesUseCases.provider)
        assertNotNull(ContileTopSitesUseCases.requireContileTopSitesProvider())

        workInfo = workManager.getWorkInfosForUniqueWork(PERIODIC_WORK_TAG).await()
        val work = workInfo.first()

        assertEquals(1, workInfo.size)
        assertEquals(WorkInfo.State.ENQUEUED, work.state)
        assertTrue(work.tags.contains(PERIODIC_WORK_TAG))
    }

    @Test
    fun `GIVEN periodic work is started WHEN period work is stopped THEN no work is queued`() = runTest {
        val updater = ContileTopSitesUpdater(testContext, provider = mock())
        val workManager = WorkManager.getInstance(testContext)
        var workInfo = workManager.getWorkInfosForUniqueWork(PERIODIC_WORK_TAG).await()

        assertTrue(workInfo.isEmpty())

        updater.startPeriodicWork()

        workInfo = workManager.getWorkInfosForUniqueWork(PERIODIC_WORK_TAG).await()

        assertEquals(1, workInfo.size)

        updater.stopPeriodicWork()

        workInfo = workManager.getWorkInfosForUniqueWork(PERIODIC_WORK_TAG).await()
        val work = workInfo.first()

        assertNull(ContileTopSitesUseCases.provider)
        assertEquals(WorkInfo.State.CANCELLED, work.state)
    }

    @Test
    fun `WHEN period work request is created THEN it contains the correct constraints`() {
        val updater = ContileTopSitesUpdater(testContext, provider = mock())
        val workRequest = updater.createPeriodicWorkRequest()

        assertTrue(workRequest.tags.contains(PERIODIC_WORK_TAG))
        assertEquals(updater.getWorkerConstraints(), workRequest.workSpec.constraints)
    }
}
