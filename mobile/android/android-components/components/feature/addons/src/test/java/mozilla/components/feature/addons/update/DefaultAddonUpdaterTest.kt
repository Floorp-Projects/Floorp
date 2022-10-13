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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.update.AddonUpdaterWorker.Companion.KEY_DATA_EXTENSIONS_ID
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.WORK_TAG_IMMEDIATE
import mozilla.components.feature.addons.update.DefaultAddonUpdater.Companion.WORK_TAG_PERIODIC
import mozilla.components.feature.addons.update.DefaultAddonUpdater.NotificationHandlerService
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class DefaultAddonUpdaterTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setUp() {
        val configuration = Configuration.Builder().build()

        // Initialize WorkManager (early) for instrumentation tests.
        WorkManagerTestInitHelper.initializeTestWorkManager(testContext, configuration)
    }

    @Test
    fun `registerForFutureUpdates - schedule work for future update`() = runTestOnMain {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val updater = DefaultAddonUpdater(testContext, frequency)
        val addonId = "addonId"

        val workId = updater.getUniquePeriodicWorkName(addonId)

        val workManger = WorkManager.getInstance(testContext)
        var workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertTrue(workData.isEmpty())

        updater.registerForFutureUpdates(addonId)
        workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertFalse(workData.isEmpty())

        assertExtensionIsRegisteredFoUpdates(updater, addonId)

        // Cleaning work manager
        workManger.cancelUniqueWork(workId)
    }

    @Test
    fun `update - schedule work for immediate update`() = runTestOnMain {
        val updater = DefaultAddonUpdater(testContext)
        val addonId = "addonId"

        val workId = updater.getUniqueImmediateWorkName(addonId)

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

    @Test
    fun `onUpdatePermissionRequest - will create a notification when user has haven't allow new permissions`() {
        val context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString(),
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

        updater.onUpdatePermissionRequest(currentExt, updatedExt, listOf("privacy")) {
            allowedPreviously = it
        }

        assertFalse(allowedPreviously)

        val notificationId = NotificationHandlerService.getNotificationId(context, currentExt.id)

        assertTrue(isNotificationVisible(notificationId))

        updater.updateStatusStorage.clear(context)
    }

    @Test
    fun `onUpdatePermissionRequest - should not show a notification for unknown permissions`() {
        val context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString(),
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

        updater.onUpdatePermissionRequest(currentExt, updatedExt, listOf("normandyAddonStudy")) {
            allowedPreviously = it
        }

        assertTrue(allowedPreviously)

        val notificationId = NotificationHandlerService.getNotificationId(context, currentExt.id)

        assertFalse(isNotificationVisible(notificationId))
        assertFalse(updater.updateStatusStorage.isPreviouslyAllowed(testContext, currentExt.id))

        updater.updateStatusStorage.clear(context)
    }

    @Test
    fun `createContentText - notification content must adapt to the amount of valid permissions`() {
        val updater = DefaultAddonUpdater(testContext)
        val validPermissions = listOf("privacy", "management")

        var content = updater.createContentText(validPermissions).split("\n")
        assertEquals("2 new permissions are required:", content[0].trim())
        assertEquals("1-Read and modify privacy settings", content[1].trim())
        assertEquals("2-Monitor extension usage and manage themes", content[2].trim())

        val validAndInvalidPermissions = listOf("privacy", "invalid")
        content = updater.createContentText(validAndInvalidPermissions).split("\n")

        assertEquals("A new permission is required:", content[0].trim())
        assertEquals("1-Read and modify privacy settings", content[1].trim())
    }

    @Test
    fun `onUpdatePermissionRequest - will NOT create a notification when permissions were granted by the user`() {
        val context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(
                ArgumentMatchers.anyString(),
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

        val notificationId = NotificationHandlerService.getNotificationId(context, currentExt.id)

        assertFalse(isNotificationVisible(notificationId))
        assertFalse(updater.updateStatusStorage.isPreviouslyAllowed(testContext, currentExt.id))
        updater.updateStatusStorage.clear(context)
    }

    @Test
    fun `createAllowAction - will create an intent with the correct addon id and allow action`() {
        val updater = spy(DefaultAddonUpdater(testContext))
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("addonId")

        updater.createAllowAction(ext, 1)

        verify(updater).createNotificationIntent(ext.id, DefaultAddonUpdater.NOTIFICATION_ACTION_ALLOW)
    }

    @Test
    fun `createDenyAction - will create an intent with the correct addon id and deny action`() {
        val updater = spy(DefaultAddonUpdater(testContext))
        val ext: WebExtension = mock()
        whenever(ext.id).thenReturn("addonId")

        updater.createDenyAction(ext, 1)

        verify(updater).createNotificationIntent(ext.id, DefaultAddonUpdater.NOTIFICATION_ACTION_DENY)
    }

    @Test
    fun `createNotificationIntent - will generate an intent with an addonId and an action`() {
        val updater = DefaultAddonUpdater(testContext)
        val addonId = "addonId"
        val action = "action"

        val intent = updater.createNotificationIntent(addonId, action)

        assertEquals(addonId, intent.getStringExtra(DefaultAddonUpdater.NOTIFICATION_EXTRA_ADDON_ID))
        assertEquals(action, intent.action)
    }

    @Test
    fun `unregisterForFutureUpdates - will remove scheduled work for future update`() = runTestOnMain {
        val frequency = Frequency(1, TimeUnit.DAYS)
        val updater = DefaultAddonUpdater(testContext, frequency)
        updater.scope = CoroutineScope(Dispatchers.Main)

        val addonId = "addonId"

        updater.updateAttempStorage = mock()

        val workId = updater.getUniquePeriodicWorkName(addonId)

        val workManger = WorkManager.getInstance(testContext)
        var workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertTrue(workData.isEmpty())

        updater.registerForFutureUpdates(addonId)
        workData = workManger.getWorkInfosForUniqueWork(workId).await()

        assertFalse(workData.isEmpty())

        assertExtensionIsRegisteredFoUpdates(updater, addonId)

        updater.unregisterForFutureUpdates(addonId)

        workData = workManger.getWorkInfosForUniqueWork(workId).await()
        assertEquals(WorkInfo.State.CANCELLED, workData.first().state)
        verify(updater.updateAttempStorage).remove(addonId)
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

    @Test
    fun `registerForFutureUpdates - will register only unregistered extensions`() = runTestOnMain {
        val updater = DefaultAddonUpdater(testContext)
        val registeredExt: WebExtension = mock()
        val notRegisteredExt: WebExtension = mock()
        whenever(registeredExt.id).thenReturn("registeredExt")
        whenever(notRegisteredExt.id).thenReturn("notRegisteredExt")

        updater.registerForFutureUpdates("registeredExt")

        val extensions = listOf(registeredExt, notRegisteredExt)

        assertExtensionIsRegisteredFoUpdates(updater, "registeredExt")

        updater.registerForFutureUpdates(extensions)

        extensions.forEach { ext ->
            assertExtensionIsRegisteredFoUpdates(updater, ext.id)
        }
    }

    @Test
    fun `registerForFutureUpdates - will not register built-in and unsupported extensions`() = runTestOnMain {
        val updater = DefaultAddonUpdater(testContext)

        val regularExt: WebExtension = mock()
        whenever(regularExt.id).thenReturn("regularExt")

        val builtInExt: WebExtension = mock()
        whenever(builtInExt.id).thenReturn("builtInExt")
        whenever(builtInExt.isBuiltIn()).thenReturn(true)

        val unsupportedExt: WebExtension = mock()
        whenever(unsupportedExt.id).thenReturn("unsupportedExt")
        val metadata: Metadata = mock()
        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(DisabledFlags.APP_SUPPORT))
        whenever(unsupportedExt.getMetadata()).thenReturn(metadata)

        val extensions = listOf(regularExt, builtInExt, unsupportedExt)
        updater.registerForFutureUpdates(extensions)

        assertExtensionIsRegisteredFoUpdates(updater, regularExt.id)

        assertExtensionIsNotRegisteredFoUpdates(updater, builtInExt.id)
        assertExtensionIsNotRegisteredFoUpdates(updater, unsupportedExt.id)
    }

    private suspend fun assertExtensionIsRegisteredFoUpdates(updater: DefaultAddonUpdater, extId: String) {
        val workId = updater.getUniquePeriodicWorkName(extId)
        val workManger = WorkManager.getInstance(testContext)
        val workData = workManger.getWorkInfosForUniqueWork(workId).await()
        val work = workData.first()

        assertEquals(WorkInfo.State.ENQUEUED, work.state)
        assertTrue(work.tags.contains(workId))
        assertTrue(work.tags.contains(WORK_TAG_PERIODIC))
    }

    private suspend fun assertExtensionIsNotRegisteredFoUpdates(updater: DefaultAddonUpdater, extId: String) {
        val workId = updater.getUniquePeriodicWorkName(extId)
        val workManger = WorkManager.getInstance(testContext)
        val workData = workManger.getWorkInfosForUniqueWork(workId).await()
        assertTrue("$extId should not have been registered for updates", workData.isEmpty())
    }

    private fun isNotificationVisible(notificationId: Int): Boolean {
        val manager = testContext.getSystemService<NotificationManager>()!!

        val notifications = manager.activeNotifications

        return notifications.any { it.id == notificationId }
    }
}
