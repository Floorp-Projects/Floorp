/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.migration

import android.app.NotificationManager
import androidx.core.content.getSystemService
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.R
import mozilla.components.feature.addons.migration.SupportedAddonsWorker.Companion.NOTIFICATION_TAG
import mozilla.components.feature.addons.ui.translateName
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class SupportedAddonsWorkerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setUp() {
        GlobalAddonDependencyProvider.addonManager = null
    }

    @After
    fun after() {
        GlobalAddonDependencyProvider.addonManager = null
    }

    @Test
    fun `doWork - will return Result_success and create a notification when a new supported add-on is found`() = runTestOnMain {
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()
        var throwable: Throwable? = null
        val onCrash = { caught: Throwable ->
            throwable = caught
        }
        val unsupportedAddon = mock<Addon> {
            whenever(translatableName).thenReturn(mapOf(Addon.DEFAULT_LOCALE to "one"))
            whenever(isSupported()).thenReturn(true)
            whenever(isDisabledAsUnsupported()).thenReturn(true)
            whenever(defaultLocale).thenReturn(Addon.DEFAULT_LOCALE)
        }

        GlobalAddonDependencyProvider.initialize(addonManager, mock(), onCrash)
        val onErrorCaptor = argumentCaptor<((Throwable) -> Unit)>()

        whenever(addonManager.getAddons()).thenReturn(listOf(unsupportedAddon))
        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.success(), result)

        val notificationId = SharedIdsHelper.getIdForTag(testContext, NOTIFICATION_TAG)
        assertTrue(isNotificationVisible(notificationId))
        verify(addonManager).enableAddon(eq(unsupportedAddon), source = eq(EnableSource.APP_SUPPORT), onSuccess = any(), onError = onErrorCaptor.capture())

        onErrorCaptor.value.invoke(Exception())
        assertNotNull(throwable!!)
    }

    @Test
    fun `doWork - will try pass any exceptions to the crashReporter`() = runTestOnMain {
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()
        var crashWasReported = false
        val crashReporter: ((Throwable) -> Unit) = { _ ->
            crashWasReported = true
        }

        GlobalAddonDependencyProvider.initialize(addonManager, mock(), crashReporter)
        GlobalAddonDependencyProvider.addonManager = null

        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.success(), result)
        assertTrue(crashWasReported)
    }

    @Test
    fun `doWork - will NOT pass any IOExceptions to the crashReporter`() = runTestOnMain {
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()
        var crashWasReported = false
        val crashReporter: ((Throwable) -> Unit) = { _ ->
            crashWasReported = true
        }

        GlobalAddonDependencyProvider.initialize(addonManager, mock(), crashReporter)

        whenever(addonManager.getAddons()).thenThrow(AddonManagerException(IOException()))
        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.success(), result)
        assertFalse(crashWasReported)
    }

    @Test
    fun `getNotificationTitle - must return the correct singular or plural version of the string`() {
        val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()

        var stringId = worker.getNotificationTitle(plural = true)
        var expectedString = testContext.getString(R.string.mozac_feature_addons_supported_checker_notification_title_plural)

        assertEquals(expectedString, stringId)

        stringId = worker.getNotificationTitle()
        expectedString = testContext.getString(R.string.mozac_feature_addons_supported_checker_notification_title)

        assertEquals(expectedString, stringId)
    }

    @Test
    fun `getNotificationBody - must return the correct singular or plural version of the string`() {
        val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()

        val appName = testContext.appName
        val firstAddon = Addon("one", translatableName = mapOf(Addon.DEFAULT_LOCALE to "one"))
        val secondAddon = Addon("two", translatableName = mapOf(Addon.DEFAULT_LOCALE to "two"))
        val thirdAddon = Addon("three", translatableName = mapOf(Addon.DEFAULT_LOCALE to "three"))

        // One add-on
        var contentString = worker.getNotificationBody(listOf(firstAddon), testContext)
        var expectedString: String? = testContext.getString(R.string.mozac_feature_addons_supported_checker_notification_content_one, firstAddon.translateName(testContext), appName)

        assertEquals(expectedString, contentString)

        // Two add-ons
        contentString = worker.getNotificationBody(listOf(firstAddon, secondAddon), testContext)
        expectedString = testContext.getString(R.string.mozac_feature_addons_supported_checker_notification_content_two, firstAddon.translateName(testContext), secondAddon.translateName(testContext), appName)

        assertEquals(expectedString, contentString)

        // More than two add-ons
        contentString = worker.getNotificationBody(listOf(firstAddon, secondAddon, thirdAddon), testContext)
        expectedString = testContext.getString(R.string.mozac_feature_addons_supported_checker_notification_content_more_than_two, appName)

        assertEquals(expectedString, contentString)

        // Zero add-ons :(
        contentString = worker.getNotificationBody(emptyList(), testContext)
        expectedString = null

        assertEquals(expectedString, contentString)
    }

    private fun isNotificationVisible(notificationId: Int): Boolean {
        val manager = testContext.getSystemService<NotificationManager>()!!

        val notifications = manager.activeNotifications

        return notifications.any { it.id == notificationId }
    }
}
