/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.WorkerParameters
import androidx.work.impl.utils.taskexecutor.TaskExecutor
import mozilla.components.service.fxa.sync.WorkManagerSyncWorker.Companion.SYNC_STAGGER_BUFFER_MS
import mozilla.components.service.fxa.sync.WorkManagerSyncWorker.Companion.engineSyncTimestamp
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class WorkManagerSyncManagerTest {
    private lateinit var mockParam: WorkerParameters
    private lateinit var mockTags: Set<String>
    private lateinit var mockTaskExecutor: TaskExecutor

    @Before
    fun setUp() {
        mockParam = mock()
        mockTags = mock()
        mockTaskExecutor = mock()
        `when`(mockParam.taskExecutor).thenReturn(mockTaskExecutor)
        `when`(mockTaskExecutor.backgroundExecutor).thenReturn(mock())
        `when`(mockParam.tags).thenReturn(mockTags)
    }

    @Test
    fun `sync state access`() {
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))

        // 'clear' doesn't blow up for empty state
        clearSyncState(testContext)
        // ... and doesn't affect anything, either
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))

        setSyncState(testContext, "some state")
        assertEquals("some state", getSyncState(testContext))

        setLastSynced(testContext, 123L)
        assertEquals(123L, getLastSynced(testContext))

        clearSyncState(testContext)
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))
    }

    @Test
    fun `GIVEN work is not set to be debounced THEN it is not considered to be synced within the buffer`() {
        `when`(mockTags.contains(SyncWorkerTag.Debounce.name)).thenReturn(false)

        engineSyncTimestamp["test"] = System.currentTimeMillis() - SYNC_STAGGER_BUFFER_MS - 100L
        engineSyncTimestamp["test2"] = System.currentTimeMillis()

        val workerManagerSyncWorker = WorkManagerSyncWorker(testContext, mockParam)

        assertFalse(workerManagerSyncWorker.isDebounced())
        assertFalse(workerManagerSyncWorker.lastSyncedWithinStaggerBuffer("test"))
        assertFalse(workerManagerSyncWorker.lastSyncedWithinStaggerBuffer("test2"))
    }

    @Test
    fun `GIVEN work is set to be debounced THEN last synced timestamp is compared to buffer`() {
        `when`(mockTags.contains(SyncWorkerTag.Debounce.name)).thenReturn(true)

        engineSyncTimestamp["test"] = System.currentTimeMillis() - SYNC_STAGGER_BUFFER_MS - 100L
        engineSyncTimestamp["test2"] = System.currentTimeMillis()

        val workerManagerSyncWorker = WorkManagerSyncWorker(testContext, mockParam)

        assert(workerManagerSyncWorker.isDebounced())
        assertFalse(workerManagerSyncWorker.lastSyncedWithinStaggerBuffer("test"))
        assert(workerManagerSyncWorker.lastSyncedWithinStaggerBuffer("test2"))
    }

    @Test
    fun `GIVEN work is set to be debounced WHEN there is not a saved time stamp THEN work will not be debounced`() {
        `when`(mockTags.contains(SyncWorkerTag.Debounce.name)).thenReturn(true)

        val workerManagerSyncWorker = WorkManagerSyncWorker(testContext, mockParam)

        assert(workerManagerSyncWorker.isDebounced())
        assertFalse(workerManagerSyncWorker.lastSyncedWithinStaggerBuffer("test"))
    }
}
