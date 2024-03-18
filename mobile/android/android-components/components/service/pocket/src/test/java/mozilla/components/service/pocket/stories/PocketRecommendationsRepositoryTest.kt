/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.ext.toPartialTimeShownUpdate
import mozilla.components.service.pocket.ext.toPocketLocalStory
import mozilla.components.service.pocket.ext.toPocketRecommendedStory
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDao
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class PocketRecommendationsRepositoryTest {

    private val pocketRepo = spy(PocketRecommendationsRepository(testContext))
    private lateinit var dao: PocketRecommendationsDao

    @Before
    fun setUp() {
        dao = mock(PocketRecommendationsDao::class.java)
        `when`(pocketRepo.pocketRecommendationsDao).thenReturn(dao)
    }

    @Test
    fun `GIVEN PocketRecommendationsRepository WHEN getPocketRecommendedStories is called THEN return db entities mapped to domain type`() {
        runTest {
            val dbStory = PocketTestResources.dbExpectedPocketStory
            `when`(dao.getPocketStories()).thenReturn(listOf(dbStory))

            val result = pocketRepo.getPocketRecommendedStories()

            verify(dao).getPocketStories()
            assertEquals(1, result.size)
            assertEquals(dbStory.toPocketRecommendedStory(), result[0])
        }
    }

    @Test
    fun `GIVEN PocketRecommendationsRepository WHEN addAllPocketApiStories is called THEN persist the received story to db`() {
        runTest {
            val apiStories = PocketTestResources.apiExpectedPocketStoriesRecommendations
            val apiStoriesMappedForDb = apiStories.map { it.toPocketLocalStory() }

            pocketRepo.addAllPocketApiStories(apiStories)

            verify(dao).cleanOldAndInsertNewPocketStories(apiStoriesMappedForDb)
        }
    }

    @Test
    fun `GIVEN PocketRecommendationsRepository WHEN updateShownPocketRecommendedStories should persist the received story to db`() {
        runTest {
            val clientStories = listOf(PocketTestResources.clientExpectedPocketStory)
            val clientStoriesPartialUpdate = clientStories.map { it.toPartialTimeShownUpdate() }

            pocketRepo.updateShownPocketRecommendedStories(clientStories)

            verify(dao).updateTimesShown(clientStoriesPartialUpdate)
        }
    }
}
