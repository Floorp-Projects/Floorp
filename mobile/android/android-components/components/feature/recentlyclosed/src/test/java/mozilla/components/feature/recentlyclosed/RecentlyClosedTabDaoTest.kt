/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import android.content.Context
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.first
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabDao
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabEntity
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabsDatabase
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class RecentlyClosedTabDaoTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: RecentlyClosedTabsDatabase
    private lateinit var tabDao: RecentlyClosedTabDao

    @Before
    fun setUp() {
        database = Room
            .inMemoryDatabaseBuilder(context, RecentlyClosedTabsDatabase::class.java)
            .allowMainThreadQueries()
            .build()
        tabDao = database.recentlyClosedTabDao()
    }

    @Test
    fun testAddingTabs() = runTestOnMain {
        val tab1 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200,
        ).also {
            tabDao.insertTab(it)
        }

        val tab2 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100,
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.getTabs().first().apply {
            assertEquals(2, this.size)
            assertEquals(tab1, this[0])
            assertEquals(tab2, this[1])
        }
        Unit
    }

    @Test
    fun testRemovingTab() = runTestOnMain {
        val tab1 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200,
        ).also {
            tabDao.insertTab(it)
        }

        val tab2 = RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100,
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.deleteTab(tab1)

        tabDao.getTabs().first().apply {
            assertEquals(1, this.size)
            assertEquals(tab2, this[0])
        }
        Unit
    }

    @Test
    fun testRemovingAllTabs() = runTestOnMain {
        RecentlyClosedTabEntity(
            title = "RecentlyClosedTab One",
            url = "https://www.mozilla.org",
            uuid = UUID.randomUUID().toString(),
            createdAt = 200,
        ).also {
            tabDao.insertTab(it)
        }

        RecentlyClosedTabEntity(
            title = "RecentlyClosedTab Two",
            url = "https://www.firefox.com",
            uuid = UUID.randomUUID().toString(),
            createdAt = 100,
        ).also {
            tabDao.insertTab(it)
        }

        tabDao.removeAllTabs()

        tabDao.getTabs().first().apply {
            assertEquals(0, this.size)
        }
        Unit
    }

    @After
    fun tearDown() {
        database.close()
    }
}
