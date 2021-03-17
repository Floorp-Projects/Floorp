/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class TabCollectionDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: TabCollectionDatabase
    private lateinit var tabCollectionDao: TabCollectionDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, TabCollectionDatabase::class.java).build()
        tabCollectionDao = database.tabCollectionDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testInsertingAndReadingCollections() {
        val collection1 = TabCollectionEntity(title = "Collection One", updatedAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", updatedAt = 50)

        tabCollectionDao.insertTabCollection(collection1)
        tabCollectionDao.insertTabCollection(collection2)

        val dataSource = tabCollectionDao.getTabCollectionsPaged()
            .create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(2, pagedList.size)

        assertEquals("Collection Two", pagedList[1]!!.collection.title)
        assertEquals("Collection One", pagedList[0]!!.collection.title)
    }

    @Test
    fun testUpdatingCollections() {
        val collection1 = TabCollectionEntity(title = "Collection One", createdAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", createdAt = 50)

        collection1.id = tabCollectionDao.insertTabCollection(collection1)
        collection2.id = tabCollectionDao.insertTabCollection(collection2)

        collection1.createdAt = 100
        collection1.title = "Updated collection"

        tabCollectionDao.updateTabCollection(collection1)

        val dataSource = tabCollectionDao.getTabCollectionsPaged()
            .create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(2, pagedList.size)

        assertEquals("Updated collection", pagedList[0]!!.collection.title)
        assertEquals("Collection Two", pagedList[1]!!.collection.title)
    }

    @Test
    fun testRemovingCollections() {
        val collection1 = TabCollectionEntity(title = "Collection One", updatedAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", updatedAt = 50)
        val collection3 = TabCollectionEntity(title = "Collection Three", updatedAt = 75)

        collection1.id = tabCollectionDao.insertTabCollection(collection1)
        collection2.id = tabCollectionDao.insertTabCollection(collection2)
        collection3.id = tabCollectionDao.insertTabCollection(collection3)

        tabCollectionDao.deleteTabCollection(collection2)

        val dataSource = tabCollectionDao.getTabCollectionsPaged()
            .create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(2, pagedList.size)

        assertEquals("Collection Three", pagedList[1]!!.collection.title)
        assertEquals("Collection One", pagedList[0]!!.collection.title)
    }

    @Test
    fun testGettingCollections() = runBlocking {
        val collection1 = TabCollectionEntity(title = "Collection One", updatedAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", updatedAt = 50)

        collection1.id = tabCollectionDao.insertTabCollection(collection1)
        collection2.id = tabCollectionDao.insertTabCollection(collection2)

        val data = tabCollectionDao.getTabCollections()

        val collections = data.first()
        assertEquals(2, collections.size)
        assertEquals("Collection Two", collections[1].collection.title)
        assertEquals("Collection One", collections[0].collection.title)
    }

    @Test
    fun testGettingCollectionsList() = runBlocking {
        val collection1 = TabCollectionEntity(title = "Collection One", updatedAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", updatedAt = 50)

        collection1.id = tabCollectionDao.insertTabCollection(collection1)
        collection2.id = tabCollectionDao.insertTabCollection(collection2)

        val tabCollections = tabCollectionDao.getTabCollectionsList()

        assertEquals(2, tabCollections.size)
        assertEquals("Collection Two", tabCollections[1].collection.title)
        assertEquals("Collection One", tabCollections[0].collection.title)
    }

    @Test
    fun testCountingTabCollections() {
        assertEquals(0, tabCollectionDao.countTabCollections())

        val collection1 = TabCollectionEntity(title = "Collection One", createdAt = 10)
        val collection2 = TabCollectionEntity(title = "Collection Two", createdAt = 50)

        collection1.id = tabCollectionDao.insertTabCollection(collection1)
        collection2.id = tabCollectionDao.insertTabCollection(collection2)

        assertEquals(2, tabCollectionDao.countTabCollections())

        tabCollectionDao.deleteTabCollection(collection2)

        assertEquals(1, tabCollectionDao.countTabCollections())
    }
}
