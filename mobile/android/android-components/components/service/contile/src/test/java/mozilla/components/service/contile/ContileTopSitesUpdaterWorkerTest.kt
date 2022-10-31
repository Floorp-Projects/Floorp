/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.spy
import java.io.IOException

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContileTopSitesUpdaterWorkerTest {

    @After
    fun cleanup() {
        ContileTopSitesUseCases.destroy()
    }

    @Test
    fun `WHEN worker does successful work THEN return a success result`() = runTest {
        val provider: ContileTopSitesProvider = mock()
        val worker = spy(
            TestListenableWorkerBuilder<ContileTopSitesUpdaterWorker>(testContext)
                .build(),
        )

        ContileTopSitesUseCases.initialize(provider)

        whenever(provider.getTopSites(anyBoolean())).thenReturn(emptyList())

        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.success(), result)
    }

    @Test
    fun `WHEN worker does unsuccessful work THEN return a failure result`() = runTest {
        val provider: ContileTopSitesProvider = mock()
        val worker = spy(
            TestListenableWorkerBuilder<ContileTopSitesUpdaterWorker>(testContext)
                .build(),
        )
        val throwable = IOException("test")

        ContileTopSitesUseCases.initialize(provider)

        whenever(provider.getTopSites(anyBoolean())).then {
            throw throwable
        }

        val result = worker.startWork().await()

        assertEquals(ListenableWorker.Result.failure(), result)
    }
}
