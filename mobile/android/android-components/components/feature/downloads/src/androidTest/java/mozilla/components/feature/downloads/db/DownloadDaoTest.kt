/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.DownloadStorage
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class DownloadDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: DownloadsDatabase
    private lateinit var dao: DownloadDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, DownloadsDatabase::class.java).build()
        dao = database.downloadDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testInsertingAndReadingDownloads() = runTest {
        val download = insertMockDownload("1", "https://www.mozilla.org/file1.txt")
        val pagedList = getDownloadsPagedList()

        assertEquals(1, pagedList.size)
        assertTrue(DownloadStorage.isSameDownload(download, pagedList[0]!!.toDownloadState()))
    }

    @Test
    fun testRemoveAllDownloads() = runTest {
        for (index in 1..4) {
            insertMockDownload(index.toString(), "https://www.mozilla.org/file1.txt")
        }

        var pagedList = getDownloadsPagedList()

        assertEquals(4, pagedList.size)
        dao.deleteAllDownloads()

        pagedList = getDownloadsPagedList()

        assertTrue(pagedList.isEmpty())
    }

    @Test
    fun testRemovingDownloads() = runTest {
        for (index in 1..2) {
            insertMockDownload(index.toString(), "https://www.mozilla.org/file1.txt")
        }

        var pagedList = getDownloadsPagedList()

        assertEquals(2, pagedList.size)

        pagedList.forEach {
            dao.delete(it)
        }

        pagedList = getDownloadsPagedList()

        assertTrue(pagedList.isEmpty())
    }

    @Test
    fun testUpdateDownload() = runTest {
        insertMockDownload("1", "https://www.mozilla.org/file1.txt")

        var pagedList = getDownloadsPagedList()

        assertEquals(1, pagedList.size)

        val download = pagedList.first()

        val updatedDownload = download.toDownloadState().copy("new_url")

        dao.update(updatedDownload.toDownloadEntity())
        pagedList = getDownloadsPagedList()

        assertEquals("new_url", pagedList.first().url)
    }

    private fun getDownloadsPagedList(): PagedList<DownloadEntity> {
        val dataSource = dao.getDownloadsPaged().create()
        return PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()
    }

    private suspend fun insertMockDownload(id: String, url: String): DownloadState {
        val download = DownloadState(
            id = id,
            url = url,
            contentType = "application/zip",
            contentLength = 5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36",
        )
        dao.insert(download.toDownloadEntity())
        return download
    }
}
