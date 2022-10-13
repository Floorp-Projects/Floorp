/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import kotlin.reflect.KVisibility

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class PocketStoriesUseCasesTest {
    private val fetchClient: Client = mock()
    private val useCases = spy(PocketStoriesUseCases(testContext, fetchClient))
    private val pocketRepo: PocketRecommendationsRepository = mock()
    private val pocketEndoint: PocketEndpoint = mock()

    @Before
    fun setup() {
        doReturn(pocketEndoint).`when`(useCases).getPocketEndpoint(any())
        doReturn(pocketRepo).`when`(useCases).getPocketRepository(any())
    }

    @Test
    fun `GIVEN a PocketStoriesUseCases THEN its visibility is internal`() {
        assertClassVisibility(PocketStoriesUseCases::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a RefreshPocketStories THEN its visibility is internal`() {
        assertClassVisibility(
            PocketStoriesUseCases.RefreshPocketStories::class,
            KVisibility.INTERNAL,
        )
    }

    @Test
    fun `GIVEN a GetPocketStories THEN its visibility is public`() {
        assertClassVisibility(PocketStoriesUseCases.GetPocketStories::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN RefreshPocketStories is constructed THEN use the same parameters`() {
        val refreshUseCase = useCases.refreshStories

        assertSame(testContext, refreshUseCase.appContext)
        assertSame(fetchClient, refreshUseCase.fetchClient)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN RefreshPocketStories is constructed separately THEN default to use the same parameters`() {
        val refreshUseCase = useCases.RefreshPocketStories()

        assertSame(testContext, refreshUseCase.appContext)
        assertSame(fetchClient, refreshUseCase.fetchClient)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN RefreshPocketStories is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()
        val fetchClient2: Client = mock()

        val refreshUseCase = useCases.RefreshPocketStories(context2, fetchClient2)

        assertSame(context2, refreshUseCase.appContext)
        assertSame(fetchClient2, refreshUseCase.fetchClient)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN RefreshPocketStories is called THEN download stories from API and return early if unsuccessful response`() = runTest {
        val refreshUseCase = useCases.RefreshPocketStories()
        val successfulResponse = getSuccessfulPocketStories()
        doReturn(successfulResponse).`when`(pocketEndoint).getRecommendedStories()

        val result = refreshUseCase.invoke()

        assertTrue(result)
        verify(pocketEndoint).getRecommendedStories()
        verify(pocketRepo).addAllPocketApiStories((successfulResponse as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN RefreshPocketStories is called THEN download stories from API and save a successful response locally`() = runTest {
        val refreshUseCase = useCases.RefreshPocketStories()
        val successfulResponse = getFailedPocketStories()
        doReturn(successfulResponse).`when`(pocketEndoint).getRecommendedStories()

        val result = refreshUseCase.invoke()

        assertFalse(result)
        verify(pocketEndoint).getRecommendedStories()
        verify(pocketRepo, never()).addAllPocketApiStories(any())
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN GetPocketStories is constructed THEN use the same parameters`() {
        val getStoriesUseCase = useCases.getStories

        assertSame(testContext, getStoriesUseCase.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN GetPocketStories is constructed separately THEN default to use the same parameters`() {
        val getStoriesUseCase = useCases.GetPocketStories()

        assertSame(testContext, getStoriesUseCase.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN GetPocketStories is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()

        val getStoriesUseCase = useCases.GetPocketStories(context2)

        assertSame(context2, getStoriesUseCase.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN GetPocketStories is called THEN delegate the repository to return locally stored stories`() =
        runTest {
            val getStoriesUseCase = useCases.GetPocketStories()
            doReturn(emptyList<PocketRecommendedStory>()).`when`(pocketRepo)
                .getPocketRecommendedStories()
            var result = getStoriesUseCase.invoke()
            verify(pocketRepo).getPocketRecommendedStories()
            assertTrue(result.isEmpty())

            val stories = listOf(PocketTestResources.clientExpectedPocketStory)
            doReturn(stories).`when`(pocketRepo).getPocketRecommendedStories()
            result = getStoriesUseCase.invoke()
            // getPocketRecommendedStories() should've been called 2 times. Once in the above check, once now.
            verify(pocketRepo, times(2)).getPocketRecommendedStories()
            assertEquals(result, stories)
        }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN UpdateStoriesTimesShown is constructed THEN use the same parameters`() {
        val updateStoriesTimesShown = useCases.updateTimesShown

        assertSame(testContext, updateStoriesTimesShown.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN UpdateStoriesTimesShown is constructed separately THEN default to use the same parameters`() {
        val updateStoriesTimesShown = useCases.UpdateStoriesTimesShown()

        assertSame(testContext, updateStoriesTimesShown.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases constructed WHEN UpdateStoriesTimesShown is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()

        val updateStoriesTimesShown = useCases.UpdateStoriesTimesShown(context2)

        assertSame(context2, updateStoriesTimesShown.context)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN UpdateStoriesTimesShown is called THEN delegate the repository to update the stories shown`() = runTest {
        val updateStoriesTimesShown = useCases.UpdateStoriesTimesShown()
        val updatedStories: List<PocketRecommendedStory> = mock()

        updateStoriesTimesShown.invoke(updatedStories)

        verify(pocketRepo).updateShownPocketRecommendedStories(updatedStories)
    }

    private fun getSuccessfulPocketStories() =
        PocketResponse.wrap(PocketTestResources.apiExpectedPocketStoriesRecommendations)

    private fun getFailedPocketStories() = PocketResponse.wrap(null)
}
