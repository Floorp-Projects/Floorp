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
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.Container
import mozilla.components.browser.state.state.ContainerState.Color
import mozilla.components.browser.state.state.ContainerState.Icon
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
    fun testAddingContainer() = runTest {
        storage.addContainer("1", "Personal", Color.RED, Icon.FINGERPRINT)
        storage.addContainer("2", "Shopping", Color.BLUE, Icon.CART)

        val containers = getAllContainers()

        assertEquals(2, containers.size)

        assertEquals("1", containers[0].contextId)
        assertEquals("Personal", containers[0].name)
        assertEquals(Color.RED, containers[0].color)
        assertEquals(Icon.FINGERPRINT, containers[0].icon)
        assertEquals("2", containers[1].contextId)
        assertEquals("Shopping", containers[1].name)
        assertEquals(Color.BLUE, containers[1].color)
        assertEquals(Icon.CART, containers[1].icon)
    }

    @Test
    fun testRemovingContainers() = runTest {
        storage.addContainer("1", "Personal", Color.RED, Icon.FINGERPRINT)
        storage.addContainer("2", "Shopping", Color.BLUE, Icon.CART)

        getAllContainers().let { containers ->
            assertEquals(2, containers.size)

            storage.removeContainer(containers[0])
        }

        getAllContainers().let { containers ->
            assertEquals(1, containers.size)

            assertEquals("2", containers[0].contextId)
            assertEquals("Shopping", containers[0].name)
            assertEquals(Color.BLUE, containers[0].color)
            assertEquals(Icon.CART, containers[0].icon)
        }
    }

    @Test
    fun testGettingContainers() = runTest {
        storage.addContainer("1", "Personal", Color.RED, Icon.FINGERPRINT)
        storage.addContainer("2", "Shopping", Color.BLUE, Icon.CART)

        val containers = storage.getContainers().first()

        assertNotNull(containers)
        assertEquals(2, containers.size)

        with(containers[0]) {
            assertEquals("1", contextId)
            assertEquals("Personal", name)
            assertEquals(Color.RED, color)
            assertEquals(Icon.FINGERPRINT, icon)
        }

        with(containers[1]) {
            assertEquals("2", contextId)
            assertEquals("Shopping", name)
            assertEquals(Color.BLUE, color)
            assertEquals(Icon.CART, icon)
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
