/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.db.DownloadsDatabase
import mozilla.components.feature.downloads.db.Migrations
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

private const val MIGRATION_TEST_DB = "migration-test"

@ExperimentalCoroutinesApi
class OnDeviceDownloadStorageTest {
    private lateinit var context: Context
    private lateinit var storage: DownloadStorage
    private lateinit var executor: ExecutorService
    private lateinit var database: DownloadsDatabase

    @get:Rule
    @Suppress("DEPRECATION")
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        DownloadsDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory(),
    )

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        database = Room.inMemoryDatabaseBuilder(context, DownloadsDatabase::class.java).build()

        storage = DownloadStorage(context)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
        database.close()
    }

    @Test
    fun migrate1to2() {
        helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            query("SELECT * FROM downloads").use { cursor ->
                assertEquals(-1, cursor.columnNames.indexOf("is_private"))
            }
            execSQL(
                "INSERT INTO " +
                    "downloads " +
                    "(id, url, file_name, content_type,content_length,status,destination_directory,created_at) " +
                    "VALUES " +
                    "(1,'url','file_name','content_type',1,1,'destination_directory',1)",
            )
        }

        val dbVersion2 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 2, true, Migrations.migration_1_2)

        dbVersion2.query("SELECT * FROM downloads").use { cursor ->
            assertTrue(cursor.columnNames.contains("is_private"))

            cursor.moveToFirst()
            assertEquals(0, cursor.getInt(cursor.getColumnIndexOrThrow("is_private")))
        }
    }

    @Test
    fun migrate2to3() {
        helper.createDatabase(MIGRATION_TEST_DB, 2).apply {
            query("SELECT * FROM downloads").use { cursor ->
                assertTrue(cursor.columnNames.contains("is_private"))
            }
            // A private download
            execSQL(
                "INSERT INTO " +
                    "downloads " +
                    "(id, url, file_name, content_type,content_length,status,destination_directory,created_at,is_private) " +
                    "VALUES " +
                    "(1,'url','file_name','content_type',1,1,'destination_directory',1,1)",
            )

            // A normal download
            execSQL(
                "INSERT INTO " +
                    "downloads " +
                    "(id, url, file_name, content_type,content_length,status,destination_directory,created_at,is_private) " +
                    "VALUES " +
                    "(2,'url','file_name','content_type',1,1,'destination_directory',1,0)",
            )
        }

        val dbVersion2 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 3, true, Migrations.migration_2_3)

        dbVersion2.query("SELECT * FROM downloads").use { cursor ->
            assertFalse(cursor.columnNames.contains("is_private"))
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            // Only non private downloads should be in the db.
            assertEquals(2, cursor.getInt(cursor.getColumnIndexOrThrow("id")))
        }
    }

    @Test
    fun migrate3to4() {
        helper.createDatabase(MIGRATION_TEST_DB, 3).apply {
            // A data url download
            execSQL(
                "INSERT INTO " +
                    "downloads " +
                    "(id, url, file_name, content_type,content_length,status,destination_directory,created_at) " +
                    "VALUES " +
                    "(1,'data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==','file_name','content_type',1,1,'destination_directory',1)",
            )
            // A normal url download
            execSQL(
                "INSERT INTO " +
                    "downloads " +
                    "(id, url, file_name, content_type,content_length,status,destination_directory,created_at) " +
                    "VALUES " +
                    "(2,'url','file_name','content_type',1,1,'destination_directory',1)",
            )
        }

        val dbVersion4 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 4, true, Migrations.migration_3_4)

        dbVersion4.query("SELECT * FROM downloads").use { cursor ->
            assertEquals(2, cursor.count)

            cursor.moveToFirst()
            // Data url must be removed from download 1.
            assertEquals("", cursor.getString(cursor.getColumnIndexOrThrow("url")))

            cursor.moveToNext()

            // The download 1 must keep its url.
            assertEquals("url", cursor.getString(cursor.getColumnIndexOrThrow("url")))
        }
    }

    @Test
    fun testAddingDownload() = runTest {
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
    fun testAddingDataURLDownload() = runTest {
        val download1 = createMockDownload("1", "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==")
        val download2 = createMockDownload("2", "url2")

        storage.add(download1)
        storage.add(download2)

        val downloads = getDownloadsPagedList()

        assertEquals(2, downloads.size)

        assertTrue(DownloadStorage.isSameDownload(download1.copy(url = ""), downloads.first()))
        assertTrue(DownloadStorage.isSameDownload(download2, downloads[1]!!))
    }

    @Test
    fun testUpdatingDataURLDownload() = runTest {
        val download1 = createMockDownload("1", "url1")
        val download2 = createMockDownload("2", "url2")

        storage.add(download1)
        storage.add(download2)

        var downloads = getDownloadsPagedList()

        assertEquals(2, downloads.size)

        assertTrue(DownloadStorage.isSameDownload(download1, downloads.first()))
        assertTrue(DownloadStorage.isSameDownload(download2, downloads[1]!!))

        val updatedDownload1 = createMockDownload("1", "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==")
        val updatedDownload2 = createMockDownload("2", "updated_url2")

        storage.update(updatedDownload1)
        storage.update(updatedDownload2)

        downloads = getDownloadsPagedList()

        assertTrue(DownloadStorage.isSameDownload(updatedDownload1.copy(url = ""), downloads.first()))
        assertTrue(DownloadStorage.isSameDownload(updatedDownload2, downloads[1]!!))
    }

    @Test
    fun testRemovingDownload() = runTest {
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
    fun testGettingDownloads() = runTest {
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
    fun testRemovingDownloads() = runTest {
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
            url = url,
            contentType = "application/zip",
            contentLength = 5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36",
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
