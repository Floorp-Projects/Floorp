/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
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
class ContainerDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: ContainerDatabase
    private lateinit var containerDao: ContainerDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, ContainerDatabase::class.java).build()
        containerDao = database.containerDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testAddingContainer() = runBlockingTest {
        val container =
            ContainerEntity(
                contextId = UUID.randomUUID().toString(),
                name = "Personal",
                color = "red",
                icon = "fingerprint"
            )
        containerDao.insertContainer(container)

        val dataSource = containerDao.getContainersPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(container, pagedList[0]!!)
    }

    @Test
    fun testRemovingContainer() = runBlockingTest {
        val container1 =
            ContainerEntity(
                contextId = UUID.randomUUID().toString(),
                name = "Personal",
                color = "red",
                icon = "fingerprint"
            )
        val container2 =
            ContainerEntity(
                contextId = UUID.randomUUID().toString(),
                name = "Shopping",
                color = "blue",
                icon = "cart"
            )

        containerDao.insertContainer(container1)
        containerDao.insertContainer(container2)
        containerDao.deleteContainer(container1)

        val dataSource = containerDao.getContainersPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(container2, pagedList[0])
    }
}
