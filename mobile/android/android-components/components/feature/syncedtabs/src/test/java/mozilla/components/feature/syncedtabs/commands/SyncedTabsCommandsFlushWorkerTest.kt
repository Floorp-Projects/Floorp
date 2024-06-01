/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import mozilla.components.concept.sync.DeviceCommandQueue
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SyncedTabsCommandsFlushWorkerTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val commands: SyncedTabsCommands = mock()

    @Before
    fun setUp() {
        GlobalSyncedTabsCommandsProvider.initialize(lazy { commands })
    }

    @Test
    fun `GIVEN a successfully flushed queue WHEN the worker runs THEN the work should complete successfully`() = runTestOnMain {
        whenever(commands.flush()).thenReturn(DeviceCommandQueue.FlushResult.ok())

        val worker = TestListenableWorkerBuilder<SyncedTabsCommandsFlushWorker>(testContext).build()
        val workerResult = worker.startWork().await()

        verify(commands).flush()
        assertEquals(ListenableWorker.Result.success(), workerResult)
    }

    @Test
    fun `GIVEN a queue that should be flushed again WHEN the worker runs THEN the work should be retried`() = runTestOnMain {
        whenever(commands.flush()).thenReturn(DeviceCommandQueue.FlushResult.retry())

        val worker = TestListenableWorkerBuilder<SyncedTabsCommandsFlushWorker>(testContext).build()
        val workerResult = worker.startWork().await()

        verify(commands).flush()
        assertEquals(ListenableWorker.Result.retry(), workerResult)
    }
}
