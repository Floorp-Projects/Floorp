/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class TopSiteDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: TopSiteDatabase
    private lateinit var topSiteDao: TopSiteDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, TopSiteDatabase::class.java).build()
        topSiteDao = database.topSiteDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testAddingTopSite() {
        val topSite = TopSiteEntity(
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 200
        ).also {
            it.id = topSiteDao.insertTopSite(it)
        }

        val dataSource = topSiteDao.getTopSitesPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(topSite, pagedList[0]!!)
    }

    @Test
    fun testRemovingTopSite() {
        val topSite1 = TopSiteEntity(
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 200
        ).also {
            it.id = topSiteDao.insertTopSite(it)
        }

        val topSite2 = TopSiteEntity(
            title = "Firefox",
            url = "https://www.firefox.com",
            createdAt = 100
        ).also {
            it.id = topSiteDao.insertTopSite(it)
        }

        topSiteDao.deleteTopSite(topSite1)

        val dataSource = topSiteDao.getTopSitesPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(topSite2, pagedList[0])
    }
}
