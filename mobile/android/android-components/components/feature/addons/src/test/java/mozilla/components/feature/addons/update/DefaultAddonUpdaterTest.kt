/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import android.app.NotificationManager
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.content.getSystemService
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
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.update.AddonUpdater.Frequency
import mozilla.components.feature.addons.update.AddonUpdaterWorker.Companion.KEY_DATA_EXTENSIONS_ID
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.NOTIFICATION_TAG
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.WORK_TAG_IMMEDIATE
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.WORK_TAG_PERIODIC
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
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
    fun `update - schedule work for immediate update`() {
        val updater = DefaultAddonUpdater(testContext)
        val addonId = "addonId"

        val workId = updater.getUniqueImmediateWorkName(addonId)

        runBlocking {
            val workManger = WorkManager.getInstance(testContext)
            var workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertTrue(workData.isEmpty())

            updater.update(addonId)
            workData = workManger.getWorkInfosForUniqueWork(workId).await()

            assertFalse(workData.isEmpty())

            val work = workData.first()

            assertEquals(WorkInfo.State.ENQUEUED, work.state)
            assertTrue(work.tags.contains(workId))
            assertTrue(work.tags.contains(WORK_TAG_IMMEDIATE))

            // Cleaning work manager
            workManger.cancelUniqueWork(workId)
        }
    }

    @Test
    fun `onUpdatePermissionRequest - will create a notification when user has haven't allow new permissions`() {
        val context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString()
            )
            doReturn(packageManager).`when`(it).packageManager
        }

        var allowedPreviously = false
        val updater = DefaultAddonUpdater(context)
        val currentExt: WebExtension = mock()
        val updatedExt: WebExtension = mock()
        whenever(currentExt.id).thenReturn("addonId")
        whenever(updatedExt.id).thenReturn("addonId")

        updater.updateStatusStorage.clear(context)

        updater.onUpdatePermissionRequest(currentExt, updatedExt, emptyList()) {
            allowedPreviously = it
        }

        assertFalse(allowedPreviously)

        val notificationId = SharedIdsHelper.getIdForTag(context, NOTIFICATION_TAG)

        assertTrue(isNotificationVisible(notificationId))

        updater.updateStatusStorage.clear(context)
    }

    @Test
    fun `onUpdatePermissionRequest - will NOT create a notification when permissions were granted by the user`() {
        val context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString()
            )
            doReturn(packageManager).`when`(it).packageManager
        }

        val updater = DefaultAddonUpdater(context)
        val currentExt: WebExtension = mock()
        val updatedExt: WebExtension = mock()
        whenever(currentExt.id).thenReturn("addonId")
        whenever(updatedExt.id).thenReturn("addonId")

        updater.updateStatusStorage.clear(context)

        var allowedPreviously = false

        updater.updateStatusStorage.markAsAllowed(context, currentExt.id)
        updater.onUpdatePermissionRequest(currentExt, updatedExt, emptyList()) {
            allowedPreviously = it
        }

        assertTrue(allowedPreviously)

        val notificationId = SharedIdsHelper.getIdForTag(context, NOTIFICATION_TAG)

        assertFalse(isNotificationVisible(notificationId))
        assertFalse(updater.updateStatusStorage.isPreviouslyAllowed(testContext, currentExt.id))
        updater.updateStatusStorage.clear(context)
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

    private fun isNotificationVisible(notificationId: Int): Boolean {
        val manager = testContext.getSystemService<NotificationManager>()!!

        val notifications = manager.activeNotifications

        return notifications.any { it.id == notificationId }
    }
}
