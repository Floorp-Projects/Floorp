/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.Configuration
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.await
import androidx.work.testing.WorkManagerTestInitHelper
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.addons.update.AddonUpdater.Frequency
import mozilla.components.feature.addons.update.AddonUpdaterWorker.Companion.KEY_DATA_EXTENSIONS_ID
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.WORK_TAG_PERIODIC
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class DefaultAddonUpdaterTest {

    @ExperimentalCoroutinesApi
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setUp() {
        val configuration = Configuration.Builder().build()

        // Initialize WorkManager (early) for instrumentation tests.
        WorkManagerTestInitHelper.initializeTestWorkManager(testContext, configuration)
    }

    @Test
    fun `registerForFutureUpdates - schedule work for future update`() {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val updater = DefaultAddonUpdater(testContext, frequency)
        val addonId = "addonId"

        val workId = updater.getUniquePeriodicWorkName(addonId)

        runBlocking {
            val workManger = WorkManager.getInstance(testContext)
            var workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertTrue(workData.isEmpty())

            updater.registerForFutureUpdates(addonId)
            workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertFalse(workData.isEmpty())

            val work = workData.first()

            assertEquals(WorkInfo.State.ENQUEUED, work.state)
            assertTrue(work.tags.contains(workId))
            assertTrue(work.tags.contains(WORK_TAG_PERIODIC))

            // Cleaning work manager
            workManger.cancelUniqueWork(workId)
        }
    }

    @Test
    fun `unregisterForFutureUpdates - will remove scheduled work for future update`() {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val updater = DefaultAddonUpdater(testContext, frequency)
        val addonId = "addonId"

        val workId = updater.getUniquePeriodicWorkName(addonId)

        runBlocking {
            val workManger = WorkManager.getInstance(testContext)
            var workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertTrue(workData.isEmpty())

            updater.registerForFutureUpdates(addonId)
            workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertFalse(workData.isEmpty())

            val work = workData.first()

            assertEquals(WorkInfo.State.ENQUEUED, work.state)
            assertTrue(work.tags.contains(workId))
            assertTrue(work.tags.contains(WORK_TAG_PERIODIC))

            updater.unregisterForFutureUpdates(addonId)

            workData = workManger.getWorkInfosForUniqueWork(workId).await()
            assertFalse(workData.isEmpty())
        }
    }

    @Test
    fun `createPeriodicWorkerRequest - will contains the right parameters`() {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val updater = DefaultAddonUpdater(testContext, frequency)
        val addonId = "addonId"

        val workId = updater.getUniquePeriodicWorkName(addonId)

        val workRequest = updater.createPeriodicWorkerRequest(addonId)

        assertTrue(workRequest.tags.contains(workId))
        assertTrue(workRequest.tags.contains(WORK_TAG_PERIODIC))

        assertEquals(updater.getWorkerConstrains(), workRequest.workSpec.constraints)

        assertEquals(addonId, workRequest.workSpec.input.getString(KEY_DATA_EXTENSIONS_ID))
    }
}
