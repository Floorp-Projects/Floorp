/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(AndroidJUnit4::class)
class SpocsDaoTest {
    private lateinit var database: PocketRecommendationsDatabase
    private lateinit var dao: SpocsDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()
        database = Room
            .inMemoryDatabaseBuilder(testContext, PocketRecommendationsDatabase::class.java)
            .allowMainThreadQueries()
            .build()
        dao = database.spocsDao()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun `GIVEN an empty table WHEN a story is inserted and then queried THEN return the same story`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc

        dao.insertSpocs(listOf(story))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different url is tried to be inserted THEN add that to the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            url = "updated" + story.url
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story, newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different title is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            title = "updated" + story.title
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different image url is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            imageUrl = "updated" + story.imageUrl
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different sponsor is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            sponsor = "updated" + story.sponsor
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different click shim is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            clickShim = "updated" + story.clickShim
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different impression shim is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            impressionShim = "updated" + story.impressionShim
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN no persisted storied WHEN asked to insert a list of stories THEN add them all to the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "2")
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "3")
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "4")

        dao.insertSpocs(listOf(story1, story2, story3, story4))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story1, story2, story3, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to delete them THEN remove all from the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "2")
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "3")
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "4")
        dao.insertSpocs(listOf(story1, story2, story3, story4))

        dao.deleteAllSpocs()
        val result = dao.getAllSpocs()

        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to cleanup and insert a new list THEN only persist the new list`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(url = story1.url + "2")
        val newStory = PocketTestResources.dbExpectedPocketSpoc.copy(url = "updated" + story1.url)
        val newStory2 = PocketTestResources.dbExpectedPocketSpoc.copy(url = "updated" + story1.url + "2")
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(newStory, newStory2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory, newStory2), result)
    }

    /**
     * Sets an executor to be used for database transactions.
     * Needs to be used along with "runTest" to ensure waiting for transactions to finish but not hang tests.
     */
    private fun setupDatabseForTransactions() {
        database = Room
            .inMemoryDatabaseBuilder(testContext, PocketRecommendationsDatabase::class.java)
            .setTransactionExecutor(executor)
            .allowMainThreadQueries()
            .build()
        dao = database.spocsDao()
    }
}
