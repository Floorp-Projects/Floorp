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
    fun `GIVEN a story already persisted WHEN another story with different id is tried to be inserted THEN add that to the table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            id = 1,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory, story), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different url is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            title = "updated" + story.url,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different title is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            title = "updated" + story.title,
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
            imageUrl = "updated" + story.imageUrl,
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
            sponsor = "updated" + story.sponsor,
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
            clickShim = "updated" + story.clickShim,
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
            impressionShim = "updated" + story.impressionShim,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with different priority is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            priority = 765,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with a different lifetime cap count is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            lifetimeCapCount = 123,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with a different flight count cap is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            flightCapCount = 999,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN a story already persisted WHEN another story with a different flight period cap is tried to be inserted THEN replace the existing`() = runTest {
        val story = PocketTestResources.dbExpectedPocketSpoc
        val newStory = story.copy(
            flightCapPeriod = 1,
        )
        dao.insertSpocs(listOf(story))

        dao.insertSpocs(listOf(newStory))
        val result = dao.getAllSpocs()

        assertEquals(listOf(newStory), result)
    }

    @Test
    fun `GIVEN no persisted storied WHEN asked to insert a list of stories THEN add them all to the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 4)

        dao.insertSpocs(listOf(story1, story2, story3, story4))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story1, story2, story3, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to delete them THEN remove all from the table`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 4)
        dao.insertSpocs(listOf(story1, story2, story3, story4))

        dao.deleteAllSpocs()
        val result = dao.getAllSpocs()

        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to delete some THEN remove remove the ones already persisted`() = runTest {
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 4)
        val story5 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 5)
        dao.insertSpocs(listOf(story1, story2, story3, story4))

        dao.deleteSpocs(listOf(story2, story3, story5))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story1, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN remove from table all stories not found in the new list`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        val story4 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 4)
        dao.insertSpocs(listOf(story1, story2, story3, story4))

        dao.cleanOldAndInsertNewSpocs(listOf(story2, story4))
        val result = dao.getAllSpocs()

        assertEquals(listOf(story2, story4), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN update stories with new ids`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            id = story1.id * 234,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        // Order gets reversed because the original story is replaced and another one is added.
        assertEquals(listOf(story2, updatedStory1), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only url changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            url = "updated" + story1.url,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only title changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            title = "updated" + story1.title,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only image url changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            imageUrl = "updated" + story1.imageUrl,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only sponsor changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            sponsor = "updated" + story1.sponsor,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only the click shim changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            clickShim = "updated" + story1.clickShim,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only the impression shim changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            impressionShim = "updated" + story1.impressionShim,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only priority changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            priority = 678,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only the lifetime count cap changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            lifetimeCapCount = 4322,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only the flight count cap changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            flightCapCount = 111111,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN stories already persisted WHEN asked to clean and insert new ones THEN don't update story if only the flight period cap changed`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val updatedStory1 = story1.copy(
            flightCapPeriod = 7,
        )
        dao.insertSpocs(listOf(story1, story2))

        dao.cleanOldAndInsertNewSpocs(listOf(updatedStory1, story2))
        val result = dao.getAllSpocs()

        assertEquals(listOf(updatedStory1, story2), result)
    }

    @Test
    fun `GIVEN no stories are persisted WHEN asked to record an impression THEN don't persist data and don't throw errors`() = runTest {
        dao.recordImpression(6543321)

        val result = dao.getSpocsImpressions()

        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN stories are persisted WHEN asked to record impressions for other stories also THEN persist impression only for existing stories`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        dao.insertSpocs(listOf(story1, story3))

        dao.recordImpressions(
            listOf(
                SpocImpressionEntity(story1.id),
                SpocImpressionEntity(story2.id),
                SpocImpressionEntity(story3.id),
            ),
        )
        val result = dao.getSpocsImpressions()

        assertEquals(2, result.size)
        assertEquals(story1.id, result[0].spocId)
        assertEquals(story3.id, result[1].spocId)
    }

    @Test
    fun `GIVEN stories are persisted WHEN asked to record impressions for existing stories THEN persist the impressions`() = runTest {
        setupDatabseForTransactions()
        val story1 = PocketTestResources.dbExpectedPocketSpoc
        val story2 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 2)
        val story3 = PocketTestResources.dbExpectedPocketSpoc.copy(id = story1.id * 3)
        dao.insertSpocs(listOf(story1, story2, story3))

        dao.recordImpressions(
            listOf(
                SpocImpressionEntity(story1.id),
                SpocImpressionEntity(story3.id),
            ),
        )
        val result = dao.getSpocsImpressions()

        assertEquals(2, result.size)
        assertEquals(story1.id, result[0].spocId)
        assertEquals(story3.id, result[1].spocId)
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
