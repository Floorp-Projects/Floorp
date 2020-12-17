/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.helpers.PocketTestResource
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class PocketRecommendationsRepositoryTest {

    @After
    fun cleanup() {
        PocketRecommendationsRepository(testContext).getStoriesFile().deleteRecursively()
    }

    @Test
    fun `GIVEN PocketRecommendationsRepository WHEN getStoriesFile is called THEN a specific File is returned`() {
        val result = PocketRecommendationsRepository(testContext).getStoriesFile()

        assertEquals(STORIES_FILE_NAME, result.name)
        assertEquals(testContext.cacheDir.toString() + "/$STORIES_FILE_NAME", result.path)
    }

    @Test
    fun `GIVEN PocketRecommendationsRepository WHEN getAtomicStoriesFile is called THEN an AtomicFile wrapping getStoriesFile is returned`() {
        val repository = spy(PocketRecommendationsRepository(testContext))
        val result = repository.getAtomicStoriesFile()

        verify(repository).getStoriesFile()
        assertEquals(repository.getStoriesFile(), result.baseFile)
    }

    @Test
    fun `GIVEN stories exist locally WHEN getPocketRecommendedStories is called THEN it returns a list of stories from the cached file`() {
        val repository = spy(PocketRecommendationsRepository(testContext))
        val jsonStringStory = PocketTestResource.ONE_POCKET_STORY_RECOMMENDATION_POCKET_RESPONSE.get()

        val result = runBlocking {
            repository.getStoriesFile().writeText(jsonStringStory)

            repository.getPocketRecommendedStories()
        }

        verify(repository).getAtomicStoriesFile()
        assertEquals(1, result.size)
        assertEquals(JSONObject(jsonStringStory).toPocketRecommendedStories(), result)
    }

    @Test
    fun `GIVEN no locally persisted stories WHEN getPocketRecommendedStories is called THEN it returns an empty list`() {
        val repository = spy(PocketRecommendationsRepository(testContext))
        val jsonStringStory = "invalid json"

        val result = runBlocking {
            repository.getStoriesFile().writeText(jsonStringStory)

            repository.getPocketRecommendedStories()
        }

        verify(repository).getAtomicStoriesFile()
        assertEquals(0, result.size)
        assertEquals(emptyList<PocketRecommendedStory>(), result)
    }

    @Test
    fun `WHEN addAllPocketPocketRecommendedStories is called THEN the received String is written to a local file`() {
        val repository = spy(PocketRecommendationsRepository(testContext))
        val jsonStringStory = PocketTestResource.ONE_POCKET_STORY_RECOMMENDATION_POCKET_RESPONSE.get()

        val persistedStories = runBlocking {
            repository.addAllPocketPocketRecommendedStories(PocketTestResource.ONE_POCKET_STORY_RECOMMENDATION_POCKET_RESPONSE.get())

            repository.getPocketRecommendedStories()
        }

        assertEquals(1, persistedStories.size)
        assertEquals(JSONObject(jsonStringStory).toPocketRecommendedStories(), persistedStories)
        // getAtomicStoriesFile should be called twice, once for writing, once for reading
        verify(repository, times(2)).getAtomicStoriesFile()
    }

    @Test
    fun `GIVEN a JSONObject representing Pocket stories response WHEN toPocketRecommendedStories is called THEN a list of PocketRecommendedStory is returned`() {
        val pocketJSONResponse = JSONObject(PocketTestResource.FIVE_POCKET_STORIES_RECOMMENDATIONS_POCKET_RESPONSE.get())

        val result = pocketJSONResponse.toPocketRecommendedStories()

        assertEquals(5, result.size)
    }

    @Test
    fun `GIVEN a JSONObject representing one Pocket story response WHEN toPocketRecommendedStory is called THEN a new PocketRecommendedStory is returned`() {
        val storyJson = JSONObject(PocketTestResource.POCKET_STORY_JSON.get())

        val result = storyJson.toPocketRecommendedStory()

        assertNotNull(result)
    }

    @Test
    fun `GIVEN an invalid JSONObject WHEN toPocketRecommendedStory is called THEN null is returned`() {
        val invalidJson = JSONObject("{}")

        val result = invalidJson.toPocketRecommendedStory()

        assertNull(result)
    }
}
