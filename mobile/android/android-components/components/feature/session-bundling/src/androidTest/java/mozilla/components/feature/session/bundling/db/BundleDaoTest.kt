/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.session.bundling.db.BundleDao
import mozilla.components.feature.session.bundling.db.BundleDatabase
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.db.UrlList
import mozilla.components.support.android.test.awaitValue
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class BundleDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: BundleDatabase
    private lateinit var bundleDao: BundleDao

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, BundleDatabase::class.java).build()
        bundleDao = database.bundleDao()
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testInsertingAndReadingBundles() {
        bundleDao.insertBundle(BundleEntity(
                id = null,
                savedAt = 100,
                urls = UrlList(listOf("https://www.mozilla.org"))))

        bundleDao.insertBundle(BundleEntity(
                id = null,
                savedAt = 200,
                urls = UrlList(listOf("https://www.firefox.com"))))

        bundleDao.insertBundle(BundleEntity(
                id = null,
                savedAt = 50,
                urls = UrlList(listOf("https://getpocket.com"))))

        bundleDao.getBundles(since = 100, limit = 10).also {
            val bundles = it.awaitValue()
            assertNotNull(bundles)

            assertEquals(2, bundles!!.size)

            val urls = bundles.toUrlList()
            assertEquals(2, urls.size)
            assertTrue(urls.contains("https://www.mozilla.org"))
            assertTrue(urls.contains("https://www.firefox.com"))
        }

        bundleDao.getBundles(since = 300, limit = 100).also {
            val bundles = it.awaitValue()
            assertNotNull(bundles)

            assertEquals(0, bundles!!.size)
        }

        bundleDao.getBundles(since = 0, limit = 0).also {
            val bundles = it.awaitValue()
            assertNotNull(bundles)

            assertEquals(0, bundles!!.size)
        }

        bundleDao.getBundles(since = 0, limit = 1).also {
            val bundles = it.awaitValue()
            assertNotNull(bundles)

            assertEquals(1, bundles!!.size)

            val urls = bundles.toUrlList()
            assertEquals(1, urls.size)
            assertTrue(urls.contains("https://www.firefox.com"))
        }
    }
}

private fun List<BundleEntity>.toUrlList(): List<String> =
    this
        .map { bundle -> bundle.urls }
        .map { urls ->
            assertEquals(1, urls.entries.size)
            urls.entries[0]
        }
