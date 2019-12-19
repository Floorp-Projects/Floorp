/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.top.sites.db.TopSiteDatabase
import mozilla.components.support.android.test.awaitValue
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@Suppress("LargeClass")
class TopSiteStorageTest {
    private lateinit var context: Context
    private lateinit var storage: TopSiteStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        val database = Room.inMemoryDatabaseBuilder(context, TopSiteDatabase::class.java).build()

        storage = TopSiteStorage(context)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
    }

    @Test
    fun testAddingTopSite() {
        storage.addTopSite("Mozilla", "https://www.mozilla.org")
        storage.addTopSite("Firefox", "https://www.firefox.com")

        val topSites = getAllTopSites()

        assertEquals(2, topSites.size)

        assertEquals("Mozilla", topSites[0].title)
        assertEquals("https://www.mozilla.org", topSites[0].url)
        assertEquals("Firefox", topSites[1].title)
        assertEquals("https://www.firefox.com", topSites[1].url)
    }

    @Test
    fun testRemovingTopSites() {
        storage.addTopSite("Mozilla", "https://www.mozilla.org")
        storage.addTopSite("Firefox", "https://www.firefox.com")

        getAllTopSites().let { topSites ->
            assertEquals(2, topSites.size)

            storage.removeTopSite(topSites[0])
        }

        getAllTopSites().let { topSites ->
            assertEquals(1, topSites.size)

            assertEquals("Firefox", topSites[0].title)
            assertEquals("https://www.firefox.com", topSites[0].url)
        }
    }

    @Test
    fun testGettingTopSites() {
        storage.addTopSite("Mozilla", "https://www.mozilla.org")
        storage.addTopSite("Firefox", "https://www.firefox.com")

        val topSites = storage.getTopSites().awaitValue()

        assertNotNull(topSites!!)
        assertEquals(2, topSites.size)

        with(topSites[0]) {
            assertEquals("Mozilla", title)
            assertEquals("https://www.mozilla.org", url)
        }

        with(topSites[1]) {
            assertEquals("Firefox", title)
            assertEquals("https://www.firefox.com", url)
        }
    }

    private fun getAllTopSites(): List<TopSite> {
        val dataSource = storage.getTopSitesPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        return pagedList.toList()
    }
}
