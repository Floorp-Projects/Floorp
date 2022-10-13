/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.helpers.PocketTestResources
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
class PocketRecommendationsDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()
    private lateinit var database: PocketRecommendationsDatabase
    private lateinit var dao: PocketRecommendationsDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()
        database = Room
            .inMemoryDatabaseBuilder(context, PocketRecommendationsDatabase::class.java)
            .allowMainThreadQueries()
            .build()
        dao = database.pocketRecommendationsDao()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun `GIVEN an empty table WHEN a story is inserted and then queried THEN return the same story`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory

        dao.insertPocketStories(listOf(story))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with identical url is tried to be inserted THEN add that to the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val newStory = story.copy(
            url = "updated" + story.url,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(newStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story, newStory), result)
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated title is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            title = "updated" + story.title,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertTrue(result.size == 1)
        assertEquals(story, result[0])
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated imageUrl is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            imageUrl = "updated" + story.imageUrl,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated publisher is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            publisher = "updated" + story.publisher,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated category is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            category = "updated" + story.category,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated timeToRead is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            timesShown = story.timesShown * 2,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN a story with the same url exists WHEN another story with updated timesShown is tried to be inserted THEN don't update the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        val updatedStory = story.copy(
            timesShown = story.timesShown * 2,
        )
        dao.insertPocketStories(listOf(story))

        dao.insertPocketStories(listOf(updatedStory))
        val result = dao.getPocketStories()

        assertEquals(listOf(story), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to delete some THEN remove them from the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val story3 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "3")
        val story4 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "4")
        dao.insertPocketStories(listOf(story1, story2, story3, story4))

        dao.delete(listOf(story2, story4))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story3), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to delete one not present in the table THEN don't update the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val story3 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "3")
        val story4 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "4")
        dao.insertPocketStories(listOf(story1, story2, story3))

        dao.delete(listOf(story4))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2, story3), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to update timesShown for one THEN update only that story`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "2",
            timesShown = story1.timesShown * 2,
        )
        val story3 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "3",
            timesShown = story1.timesShown * 3,
        )
        val story4 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "4",
            timesShown = story1.timesShown * 4,
        )
        val updatedStory2 = PocketLocalStoryTimesShown(story2.url, 222)
        val updatedStory4 = PocketLocalStoryTimesShown(story4.url, 444)
        dao.insertPocketStories(listOf(story1, story2, story3, story4))

        dao.updateTimesShown(listOf(updatedStory2, updatedStory4))
        val result = dao.getPocketStories()

        assertEquals(
            listOf(
                story1,
                story2.copy(timesShown = 222),
                story3,
                story4.copy(timesShown = 444),
            ),
            result,
        )
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to update timesShown for one not present in the table THEN don't update the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "2",
            timesShown = story1.timesShown * 2,
        )
        val story3 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "3",
            timesShown = story1.timesShown * 3,
        )
        val story4 = PocketTestResources.dbExpectedPocketStory.copy(
            url = story1.url + "4",
            timesShown = story1.timesShown * 4,
        )
        val otherStoryUpdateDetails = PocketLocalStoryTimesShown("differentUrl", 111)
        dao.insertPocketStories(listOf(story1, story2, story3, story4))

        dao.updateTimesShown(listOf(otherStoryUpdateDetails))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2, story3, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN remove from table all stories not found in the new list`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val story3 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "3")
        val story4 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "4")
        dao.insertPocketStories(listOf(story1, story2, story3, story4))

        dao.cleanOldAndInsertNewPocketStories(listOf(story2, story4))
        val result = dao.getPocketStories()

        assertEquals(listOf(story2, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN update stories with new urls`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory1 = story1.copy(
            url = "updated" + story1.url,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(updatedStory1, story2))
        val result = dao.getPocketStories()

        // Order gets reversed because the original story is replaced and another one is added.
        assertEquals(listOf(story2, updatedStory1), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN update stories with new image urls`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory2 = story2.copy(
            imageUrl = "updated" + story2.url,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(story1, updatedStory2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, updatedStory2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only title changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory1 = story1.copy(
            title = "updated" + story1.title,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(updatedStory1, story2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only publisher changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory2 = story2.copy(
            publisher = "updated" + story2.publisher,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(story1, updatedStory2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only category changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory1 = story1.copy(
            category = "updated" + story1.category,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(updatedStory1, story2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only timeToRead changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory1 = story1.copy(
            timeToRead = story1.timeToRead * 2,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(updatedStory1, story2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only timesShown changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketStory
        val story2 = PocketTestResources.dbExpectedPocketStory.copy(url = story1.url + "2")
        val updatedStory2 = story2.copy(
            timesShown = story2.timesShown * 2,
        )
        dao.insertPocketStories(listOf(story1, story2))

        dao.cleanOldAndInsertNewPocketStories(listOf(story1, updatedStory2))
        val result = dao.getPocketStories()

        assertEquals(listOf(story1, story2), result)
    }

    /**
     * Sets an executor to be used for database transactions.
     * Needs to be used along with "runTest" to ensure waiting for transactions to finish but not hang tests.
     */
    private fun setupDatabseForTransactions() {
        database = Room
            .inMemoryDatabaseBuilder(context, PocketRecommendationsDatabase::class.java)
            .setTransactionExecutor(executor)
            .allowMainThreadQueries()
            .build()
        dao = database.pocketRecommendationsDao()
    }
}
