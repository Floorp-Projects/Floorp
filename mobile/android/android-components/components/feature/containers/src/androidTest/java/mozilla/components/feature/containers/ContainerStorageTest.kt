/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.feature.containers.db.ContainerDatabase
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@ExperimentalCoroutinesApi
@Suppress("LargeClass")
class ContainerStorageTest {
    private lateinit var context: Context
    private lateinit var storage: ContainerStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        val database = Room.inMemoryDatabaseBuilder(context, ContainerDatabase::class.java).build()

        storage = ContainerStorage(context)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
    }

    @Test
    fun testAddingContainer() = runBlockingTest {
        storage.addContainer("Personal", "red", "fingerprint")
        storage.addContainer("Shopping", "blue", "cart")

        val containers = getAllContainers()

        assertEquals(2, containers.size)

        assertEquals("Personal", containers[0].name)
        assertEquals("red", containers[0].color)
        assertEquals("fingerprint", containers[0].icon)
        assertEquals("Shopping", containers[1].name)
        assertEquals("blue", containers[1].color)
        assertEquals("cart", containers[1].icon)
    }

    @Test
    fun testRemovingContainers() = runBlockingTest {
        storage.addContainer("Personal", "red", "fingerprint")
        storage.addContainer("Shopping", "blue", "cart")

        getAllContainers().let { containers ->
            assertEquals(2, containers.size)

            storage.removeContainer(containers[0])
        }

        getAllContainers().let { containers ->
            assertEquals(1, containers.size)

            assertEquals("Shopping", containers[0].name)
            assertEquals("blue", containers[0].color)
            assertEquals("cart", containers[0].icon)
        }
    }

    @Test
    fun testGettingContainers() = runBlockingTest {
        storage.addContainer("Personal", "red", "fingerprint")
        storage.addContainer("Shopping", "blue", "cart")

        val containers = storage.getContainers().first()

        assertNotNull(containers)
        assertEquals(2, containers.size)

        with(containers[0]) {
            assertEquals("Personal", name)
            assertEquals("red", color)
            assertEquals("fingerprint", icon)
        }

        with(containers[1]) {
            assertEquals("Shopping", name)
            assertEquals("blue", color)
            assertEquals("cart", icon)
        }
    }

    private fun getAllContainers(): List<Container> {
        val dataSource = storage.getContainersPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        return pagedList.toList()
    }
}
