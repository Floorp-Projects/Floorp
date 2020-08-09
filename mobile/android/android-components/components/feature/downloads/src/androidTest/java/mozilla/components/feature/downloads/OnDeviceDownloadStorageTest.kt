/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.db.DownloadsDatabase
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@ExperimentalCoroutinesApi
class OnDeviceDownloadStorageTest {
    private lateinit var context: Context
    private lateinit var storage: DownloadStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        val database = Room.inMemoryDatabaseBuilder(context, DownloadsDatabase::class.java).build()

        storage = DownloadStorage(context)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
    }

    @Test
    fun testAddingDownload() = runBlockingTest {
        val download1 = createMockDownload("1", "url1")
        val download2 = createMockDownload("2", "url2")
        val download3 = createMockDownload("3", "url3")

        storage.add(download1)
        storage.add(download2)
        storage.add(download3)

        val downloads = getDownloadsPagedList()

        assertEquals(3, downloads.size)

        assertTrue(DownloadStorage.isSameDownload(download1, downloads.first()))
        assertTrue(DownloadStorage.isSameDownload(download2, downloads[1]!!))
        assertTrue(DownloadStorage.isSameDownload(download3, downloads[2]!!))
    }

    @Test
    fun testRemovingDownload() = runBlockingTest {
        val download1 = createMockDownload("1", "url1")
        val download2 = createMockDownload("2", "url2")

        storage.add(download1)
        storage.add(download2)

        assertEquals(2, getDownloadsPagedList().size)

        storage.remove(download1)

        val downloads = getDownloadsPagedList()
        val downloadFromDB = downloads.first()

        assertEquals(1, downloads.size)
        assertTrue(DownloadStorage.isSameDownload(download2, downloadFromDB))
    }

    @Test
    fun testGettingDownloads() = runBlockingTest {
        val download1 = createMockDownload("1", "url1")
        val download2 = createMockDownload("2", "url2")

        storage.add(download1)
        storage.add(download2)

        val downloads = getDownloadsPagedList()

        assertEquals(2, downloads.size)

        assertTrue(DownloadStorage.isSameDownload(download1, downloads.first()))
        assertTrue(DownloadStorage.isSameDownload(download2, downloads[1]!!))
    }

    @Test
    fun testRemovingDownloads() = runBlocking {
        for (index in 1..2) {
            storage.add(createMockDownload(index.toString(), "url1"))
        }

        var pagedList = getDownloadsPagedList()

        assertEquals(2, pagedList.size)

        pagedList.forEach {
            storage.remove(it)
        }

        pagedList = getDownloadsPagedList()

        assertTrue(pagedList.isEmpty())
    }

    private fun createMockDownload(id: String, url: String): DownloadState {
        return DownloadState(
                id = id,
                url = url, contentType = "application/zip", contentLength = 5242880,
                userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
        )
    }

    private fun getDownloadsPagedList(): PagedList<DownloadState> {
        val dataSource = storage.getDownloadsPaged().create()
        return PagedList.Builder(dataSource, 10)
                .setNotifyExecutor(executor)
                .setFetchExecutor(executor)
                .build()
    }
}
