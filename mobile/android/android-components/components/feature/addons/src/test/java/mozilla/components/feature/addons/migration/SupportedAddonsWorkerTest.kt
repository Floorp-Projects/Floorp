/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
    fun `doWork - will return Result_success and update new supported add-ons when found`() =
        runTestOnMain {
            val addonManager = mock<AddonManager>()
            val addonUpdater = mock<AddonUpdater>()
            val worker = TestListenableWorkerBuilder<SupportedAddonsWorker>(testContext).build()
            val unsupportedAddon = mock<Addon> {
                whenever(translatableName).thenReturn(mapOf(Addon.DEFAULT_LOCALE to "one"))
                whenever(isSupported()).thenReturn(true)
                whenever(isDisabledAsUnsupported()).thenReturn(true)
                whenever(defaultLocale).thenReturn(Addon.DEFAULT_LOCALE)
            }

            GlobalAddonDependencyProvider.initialize(addonManager, addonUpdater, mock())

            whenever(addonManager.getAddons()).thenReturn(listOf(unsupportedAddon))
            val result = worker.startWork().await()

            assertEquals(ListenableWorker.Result.success(), result)

            verify(addonUpdater).registerForFutureUpdates(unsupportedAddon.id)
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
}
