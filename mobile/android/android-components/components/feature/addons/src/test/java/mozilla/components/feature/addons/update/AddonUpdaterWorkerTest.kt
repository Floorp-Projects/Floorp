/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import junit.framework.TestCase.assertEquals
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdaterWorker
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString

@RunWith(AndroidJUnit4::class)
class AddonUpdaterWorkerTest {

    @ExperimentalCoroutinesApi
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
    fun `doWork - will return Result_success when SuccessfullyUpdated`() {
        val addonId = "addonId"
        val onFinishCaptor = argumentCaptor<((AddonUpdater.Status) -> Unit)>()
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<AddonUpdaterWorker>(testContext)
            .setInputData(AddonUpdaterWorker.createWorkerData(addonId))
            .build()

        GlobalAddonDependencyProvider.initialize(addonManager, mock())

        whenever(addonManager.updateAddon(anyString(), onFinishCaptor.capture())).then {
            onFinishCaptor.value.invoke(AddonUpdater.Status.SuccessfullyUpdated)
        }

        runBlocking {
            val result = worker.startWork().await()

            assertEquals(ListenableWorker.Result.success(), result)
        }
    }

    @Test
    fun `doWork - will return Result_success when NoUpdateAvailable`() {
        val addonId = "addonId"
        val onFinishCaptor = argumentCaptor<((AddonUpdater.Status) -> Unit)>()
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<AddonUpdaterWorker>(testContext)
            .setInputData(AddonUpdaterWorker.createWorkerData(addonId))
            .build()

        GlobalAddonDependencyProvider.initialize(addonManager, mock())

        whenever(addonManager.updateAddon(anyString(), onFinishCaptor.capture())).then {
            onFinishCaptor.value.invoke(AddonUpdater.Status.NoUpdateAvailable)
        }

        runBlocking {
            val result = worker.startWork().await()

            assertEquals(ListenableWorker.Result.success(), result)
        }
    }

    @Test
    fun `doWork - will return Result_failure when NotInstalled`() {
        val addonId = "addonId"
        val onFinishCaptor = argumentCaptor<((AddonUpdater.Status) -> Unit)>()
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<AddonUpdaterWorker>(testContext)
            .setInputData(AddonUpdaterWorker.createWorkerData(addonId))
            .build()

        GlobalAddonDependencyProvider.initialize(addonManager, mock())

        whenever(addonManager.updateAddon(anyString(), onFinishCaptor.capture())).then {
            onFinishCaptor.value.invoke(AddonUpdater.Status.NotInstalled)
        }

        runBlocking {
            val result = worker.startWork().await()

            assertEquals(ListenableWorker.Result.failure(), result)
        }
    }

    @Test
    fun `doWork - will return Result_retry when an Error happens`() {
        val addonId = "addonId"
        val onFinishCaptor = argumentCaptor<((AddonUpdater.Status) -> Unit)>()
        val addonManager = mock<AddonManager>()
        val worker = TestListenableWorkerBuilder<AddonUpdaterWorker>(testContext)
            .setInputData(AddonUpdaterWorker.createWorkerData(addonId))
            .build()

        GlobalAddonDependencyProvider.initialize(addonManager, mock())

        whenever(addonManager.updateAddon(anyString(), onFinishCaptor.capture())).then {
            onFinishCaptor.value.invoke(AddonUpdater.Status.Error("error", Exception()))
        }

        runBlocking {
            val result = worker.startWork().await()

            assertEquals(ListenableWorker.Result.retry(), result)
        }
    }
}
