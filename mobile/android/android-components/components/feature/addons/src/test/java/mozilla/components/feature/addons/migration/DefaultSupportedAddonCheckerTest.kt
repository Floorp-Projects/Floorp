/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.migration

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.Configuration
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.await
import androidx.work.testing.WorkManagerTestInitHelper
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker.Companion.CHECKER_UNIQUE_PERIODIC_WORK_NAME
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker.Companion.WORK_TAG_PERIODIC
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class DefaultSupportedAddonCheckerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    lateinit var context: Context

    @Before
    fun setUp() {
        val configuration = Configuration.Builder().build()
        context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString(),
            )
            doReturn(packageManager).`when`(it).packageManager
        }

        // Initialize WorkManager (early) for instrumentation tests.
        WorkManagerTestInitHelper.initializeTestWorkManager(testContext, configuration)
        SupportedAddonsWorker.onNotificationClickIntent = Intent()
    }

    @Test
    fun `registerForChecks - schedule work for future checks`() = runTestOnMain {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val intent = Intent()
        val checker = DefaultSupportedAddonsChecker(context, frequency, intent)

        val workId = CHECKER_UNIQUE_PERIODIC_WORK_NAME

        val workManger = WorkManager.getInstance(testContext)
        val workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertTrue(workData.isEmpty())

        checker.registerForChecks()

        assertExtensionIsRegisteredForChecks()
        assertEquals(intent, SupportedAddonsWorker.onNotificationClickIntent)
        // Cleaning work manager
        workManger.cancelUniqueWork(workId)
    }

    @Test
    fun `unregisterForChecks - will remove scheduled work for future checks`() = runTestOnMain {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val checker = DefaultSupportedAddonsChecker(context, frequency)

        val workId = CHECKER_UNIQUE_PERIODIC_WORK_NAME

        val workManger = WorkManager.getInstance(testContext)
        var workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertTrue(workData.isEmpty())

        checker.registerForChecks()

        assertExtensionIsRegisteredForChecks()

        checker.unregisterForChecks()

        workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertEquals(WorkInfo.State.CANCELLED, workData.first().state)
    }

    private suspend fun assertExtensionIsRegisteredForChecks() {
        val workId = CHECKER_UNIQUE_PERIODIC_WORK_NAME
        val workManger = WorkManager.getInstance(testContext)
        val workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertFalse(workData.isEmpty())

        val work = workData.first()

        assertEquals(WorkInfo.State.ENQUEUED, work.state)
        assertTrue(work.tags.contains(workId))
        assertTrue(work.tags.contains(WORK_TAG_PERIODIC))
    }
}
