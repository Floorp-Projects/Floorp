/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabEntity
import mozilla.components.feature.recentlyclosed.db.RecentlyClosedTabsDatabase
import mozilla.components.support.ktx.java.io.truncateDirectory
import mozilla.components.support.test.fakes.engine.FakeEngine
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@ExperimentalCoroutinesApi
class RecentlyClosedTabsStorageTest {
    private lateinit var context: Context
    private lateinit var storage: RecentlyClosedTabsStorage
    private lateinit var executor: ExecutorService
    private lateinit var dispatcher: TestCoroutineDispatcher
    private lateinit var engine: Engine

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()
        context = ApplicationProvider.getApplicationContext()

        val database =
            Room.inMemoryDatabaseBuilder(context, RecentlyClosedTabsDatabase::class.java).build()

        dispatcher = TestCoroutineDispatcher()
        val scope = CoroutineScope(dispatcher)

        engine = FakeEngine()
        storage = RecentlyClosedTabsStorage(context, engine, scope)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        RecentlyClosedTabEntity.getStateDirectory(context.filesDir).truncateDirectory()

        executor.shutdown()
    }

    @Test
    fun testAddingTabsWithMax() = runBlockingTest {
        // Test tab
        val closedTab = RecoverableTab(
            id = "first-tab",
            title = "Mozilla",
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis()
        )

        // Test tab
        val secondClosedTab = RecoverableTab(
            id = "second-tab",
            title = "Pocket",
            url = "https://pocket.com",
            lastAccess = System.currentTimeMillis()
        )

        storage.addTabsToCollectionWithMax(listOf(closedTab, secondClosedTab), 1)

        dispatcher.advanceUntilIdle()

        val tabs = storage.getTabs().first()

        Assert.assertEquals(1, tabs.size)
        Assert.assertEquals(secondClosedTab.url, tabs[0].url)
        Assert.assertEquals(secondClosedTab.title, tabs[0].title)
        Assert.assertEquals(secondClosedTab.lastAccess, tabs[0].lastAccess)

        // Test tab
        val thirdClosedTab = RecoverableTab(
            id = "third-tab",
            title = "Firefox",
            url = "https://firefox.com",
            lastAccess = System.currentTimeMillis()
        )

        storage.addTabsToCollectionWithMax(listOf(thirdClosedTab), 1)

        dispatcher.advanceUntilIdle()

        val newTabs = storage.getTabs().first()

        Assert.assertEquals(1, newTabs.size)
        Assert.assertEquals(thirdClosedTab.url, newTabs[0].url)
        Assert.assertEquals(thirdClosedTab.title, newTabs[0].title)
        Assert.assertEquals(thirdClosedTab.lastAccess, newTabs[0].lastAccess)
    }

    @Test
    fun testAllowAddingSameTabTwice() = runBlockingTest {
        // Test tab
        val closedTab = RecoverableTab(
            id = "first-tab",
            title = "Mozilla",
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis()
        )

        storage.addTabsToCollectionWithMax(listOf(closedTab), 2)
        dispatcher.advanceUntilIdle()

        val updatedTab = closedTab.copy(title = "updated")
        storage.addTabsToCollectionWithMax(listOf(updatedTab), 2)
        dispatcher.advanceUntilIdle()

        val tabs = storage.getTabs().first()

        Assert.assertEquals(1, tabs.size)
        Assert.assertEquals(updatedTab.url, tabs[0].url)
        Assert.assertEquals(updatedTab.title, tabs[0].title)
        Assert.assertEquals(updatedTab.lastAccess, tabs[0].lastAccess)
    }

    @Test
    fun testRemovingAllTabs() = runBlockingTest {
        // Test tab
        val closedTab = RecoverableTab(
            id = "first-tab",
            title = "Mozilla",
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis()
        )

        // Test tab
        val secondClosedTab = RecoverableTab(
            id = "second-tab",
            title = "Pocket",
            url = "https://pocket.com",
            lastAccess = System.currentTimeMillis()
        )

        storage.addTabsToCollectionWithMax(listOf(closedTab, secondClosedTab), 2)

        dispatcher.advanceUntilIdle()

        val tabs = storage.getTabs().first()

        Assert.assertEquals(2, tabs.size)
        Assert.assertEquals(closedTab.url, tabs[0].url)
        Assert.assertEquals(closedTab.title, tabs[0].title)
        Assert.assertEquals(closedTab.lastAccess, tabs[0].lastAccess)
        Assert.assertEquals(secondClosedTab.url, tabs[1].url)
        Assert.assertEquals(secondClosedTab.title, tabs[1].title)
        Assert.assertEquals(secondClosedTab.lastAccess, tabs[1].lastAccess)

        storage.removeAllTabs()

        dispatcher.advanceUntilIdle()

        val newTabs = storage.getTabs().first()

        Assert.assertEquals(0, newTabs.size)
    }

    @Test
    fun testRemovingOneTab() = runBlockingTest {
        // Test tab
        val closedTab = RecoverableTab(
            id = "first-tab",
            title = "Mozilla",
            url = "https://mozilla.org",
            lastAccess = System.currentTimeMillis()
        )

        // Test tab
        val secondClosedTab = RecoverableTab(
            id = "second-tab",
            title = "Pocket",
            url = "https://pocket.com",
            lastAccess = System.currentTimeMillis()
        )

        storage.addTabState(closedTab)
        storage.addTabState(secondClosedTab)

        dispatcher.advanceUntilIdle()

        val tabs = storage.getTabs().first()

        Assert.assertEquals(2, tabs.size)
        Assert.assertEquals(closedTab.url, tabs[0].url)
        Assert.assertEquals(closedTab.title, tabs[0].title)
        Assert.assertEquals(closedTab.lastAccess, tabs[0].lastAccess)
        Assert.assertEquals(secondClosedTab.url, tabs[1].url)
        Assert.assertEquals(secondClosedTab.title, tabs[1].title)
        Assert.assertEquals(secondClosedTab.lastAccess, tabs[1].lastAccess)

        storage.removeTab(tabs[0])

        dispatcher.advanceUntilIdle()

        val newTabs = storage.getTabs().first()

        Assert.assertEquals(1, newTabs.size)
        Assert.assertEquals(secondClosedTab.url, newTabs[0].url)
        Assert.assertEquals(secondClosedTab.title, newTabs[0].title)
        Assert.assertEquals(secondClosedTab.lastAccess, newTabs[0].lastAccess)
    }
}
