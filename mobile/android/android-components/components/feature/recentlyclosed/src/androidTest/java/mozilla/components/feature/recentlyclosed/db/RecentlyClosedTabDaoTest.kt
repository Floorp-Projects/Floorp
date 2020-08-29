/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.test.runBlockingTest
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.UUID
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@ExperimentalCoroutinesApi
class RecentlyClosedTabDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: RecentlyClosedTabsDatabase
    private lateinit var tabDao: RecentlyClosedTabDao
    private lateinit var executor: ExecutorService

    @get:Rule
    val instantExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database =
            Room.inMemoryDatabaseBuilder(context, RecentlyClosedTabsDatabase::class.java).build()
        tabDao = database.recentlyClosedTabDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @Test
    fun testAddingTabs() = runBlockingTest {
        val tab1 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200
        ).also {
            tabDao.insertTab(it)
        }

        val tab2 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.getTabs().first().apply {
            assertEquals(2, this.size)
            assertEquals(tab1, this[0])
            assertEquals(tab2, this[1])
        }
    }

    @Test
    fun testRemovingTab() = runBlockingTest {
        val tab1 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200
        ).also {
            tabDao.insertTab(it)
        }

        val tab2 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.deleteTab(tab1)

        tabDao.getTabs().first().apply {
            assertEquals(1, this.size)
            assertEquals(tab2, this[0])
        }
    }

    @Test
    fun testRemovingAllTabs() = runBlockingTest {
        RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200
        ).also {
            tabDao.insertTab(it)
        }

        RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.removeAllTabs()

        tabDao.getTabs().first().apply {
            assertEquals(0, this.size)
        }
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }
}
