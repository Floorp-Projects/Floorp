/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class PinnedSiteDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: TopSiteDatabase
    private lateinit var pinnedSiteDao: PinnedSiteDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, TopSiteDatabase::class.java).build()
        pinnedSiteDao = database.pinnedSiteDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testAddingTopSite() {
        val topSite = PinnedSiteEntity(
            title = "Mozilla",
            url = "https://www.mozilla.org",
            isDefault = false,
            createdAt = 200,
        ).also {
            it.id = pinnedSiteDao.insertPinnedSite(it)
        }

        val pinnedSites = pinnedSiteDao.getPinnedSites()

        assertEquals(1, pinnedSites.size)
        assertEquals(1, pinnedSiteDao.getPinnedSitesCount())
        assertEquals(topSite, pinnedSites[0])
    }

    @Test
    fun testUpdatingTopSite() {
        val topSite = PinnedSiteEntity(
            title = "Mozilla",
            url = "https://www.mozilla.org",
            isDefault = false,
            createdAt = 200,
        ).also {
            it.id = pinnedSiteDao.insertPinnedSite(it)
        }

        topSite.title = "Mozilla (IT)"
        topSite.url = "https://www.mozilla.org/it"
        pinnedSiteDao.updatePinnedSite(topSite)

        val pinnedSites = pinnedSiteDao.getPinnedSites()

        assertEquals(1, pinnedSites.size)
        assertEquals(1, pinnedSiteDao.getPinnedSitesCount())
        assertEquals(topSite, pinnedSites[0])
        assertEquals(topSite.title, pinnedSites[0].title)
        assertEquals(topSite.url, pinnedSites[0].url)
    }

    @Test
    fun testRemovingTopSite() {
        val topSite1 = PinnedSiteEntity(
            title = "Mozilla",
            url = "https://www.mozilla.org",
            isDefault = false,
            createdAt = 200,
        ).also {
            it.id = pinnedSiteDao.insertPinnedSite(it)
        }

        val topSite2 = PinnedSiteEntity(
            title = "Firefox",
            url = "https://www.firefox.com",
            isDefault = false,
            createdAt = 100,
        ).also {
            it.id = pinnedSiteDao.insertPinnedSite(it)
        }

        var pinnedSites = pinnedSiteDao.getPinnedSites()

        assertEquals(2, pinnedSites.size)
        assertEquals(2, pinnedSiteDao.getPinnedSitesCount())

        pinnedSiteDao.deletePinnedSite(topSite1)

        pinnedSites = pinnedSiteDao.getPinnedSites()

        assertEquals(1, pinnedSites.size)
        assertEquals(1, pinnedSiteDao.getPinnedSitesCount())
        assertEquals(topSite2, pinnedSites[0])
    }
}
