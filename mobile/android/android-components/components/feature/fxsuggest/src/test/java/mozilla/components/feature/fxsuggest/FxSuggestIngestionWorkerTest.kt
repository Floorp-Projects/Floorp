/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.fxsuggest

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import mozilla.appservices.suggest.SuggestApiException
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class FxSuggestIngestionWorkerTest {
    private lateinit var storage: FxSuggestStorage

    @Before
    fun setUp() {
        storage = mock()
        GlobalFxSuggestDependencyProvider.storage = storage
    }

    @After
    fun tearDown() {
        GlobalFxSuggestDependencyProvider.storage = null
    }

    @Test
    fun workSucceeds() = runTest {
        whenever(storage.ingest(any())).thenReturn(Unit)

        val worker = TestListenableWorkerBuilder<FxSuggestIngestionWorker>(testContext).build()

        val result = worker.startWork().await()

        verify(storage).ingest(any())
        assertEquals(ListenableWorker.Result.success(), result)
    }

    @Test
    fun workShouldRetry() = runTest {
        whenever(storage.ingest(any())).then {
            throw SuggestApiException.Other("Mercury in retrograde")
        }

        val worker = TestListenableWorkerBuilder<FxSuggestIngestionWorker>(testContext).build()

        val result = worker.startWork().await()

        verify(storage).ingest(any())
        assertEquals(ListenableWorker.Result.retry(), result)
    }

    @Test
    fun workShouldRethrow() = runTest {
        whenever(storage.ingest(any())).then {
            throw IOException()
        }

        val worker = TestListenableWorkerBuilder<FxSuggestIngestionWorker>(testContext).build()

        assertThrows(IOException::class.java) {
            runBlocking {
                worker.startWork().await()
            }
        }
        verify(storage).ingest(any())
    }
}
