/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.PocketSponsoredStory
import mozilla.components.service.pocket.PocketSponsoredStoryShim
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.spocs.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
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
import java.util.UUID
import kotlin.reflect.KVisibility

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class PocketStoriesUseCasesTest {
    private val usecases = spy(PocketStoriesUseCases())
    private val pocketRepo: PocketRecommendationsRepository = mock()
    private val pocketEndoint: PocketEndpoint = mock()
    private val spocsProvider: SpocsEndpoint = mock()

    @Before
    fun setup() {
        doReturn(pocketEndoint).`when`(usecases).getPocketEndpoint(any())
        doReturn(pocketRepo).`when`(usecases).getPocketRepository(any())
        doReturn(spocsProvider).`when`(usecases).getSpocsProvider(any(), any(), any())
    }

    @After
    fun cleanup() {
        PocketStoriesUseCases.reset()
    }

    @Test
    fun `GIVEN a PocketStoriesUseCases THEN its visibility is public`() {
        assertClassVisibility(PocketStoriesUseCases::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN a RefreshPocketStories THEN its visibility is internal`() {
        assertClassVisibility(
            PocketStoriesUseCases.RefreshPocketStories::class,
            KVisibility.INTERNAL
        )
    }

    @Test
    fun `GIVEN a GetPocketStories THEN its visibility is public`() {
        assertClassVisibility(PocketStoriesUseCases.GetPocketStories::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a GetSponsoredStories THEN its visibility is internal`() {
        assertClassVisibility(PocketStoriesUseCases.GetSponsoredStories::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a DeleteUserProfile THEN its visibility is internal`() {
        assertClassVisibility(PocketStoriesUseCases.DeleteUserProfile::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN an uninitialized PocketStoriesUseCases WHEN initialize is called THEN api key and client are set`() {
        val client: Client = mock()

        PocketStoriesUseCases.initialize(client)

        assertSame(client, PocketStoriesUseCases.fetchClient)
    }

    @Test
    fun `GIVEN an already initialized PocketStoriesUseCases WHEN initialize is called THEN the new api key and client overwrite the old values`() {
        PocketStoriesUseCases.fetchClient = mock()
        val newClient: Client = mock()

        PocketStoriesUseCases.initialize(newClient)

        assertSame(newClient, PocketStoriesUseCases.fetchClient)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN reset is called THEN the api key and fetch client references are freed`() {
        PocketStoriesUseCases.fetchClient = mock()

        PocketStoriesUseCases.reset()

        assertNull(PocketStoriesUseCases.fetchClient)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN RefreshPocketStories is called THEN download stories from API and return early if unsuccessful response`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val refreshUsecase = spy(
            usecases.RefreshPocketStories(testContext)
        )
        val successfulResponse = getSuccessfulPocketStories()
        doReturn(successfulResponse).`when`(pocketEndoint).getRecommendedStories()

        val result = refreshUsecase.invoke()

        assertTrue(result)
        verify(pocketEndoint).getRecommendedStories()
        verify(pocketRepo).addAllPocketApiStories((successfulResponse as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN RefreshPocketStories is called THEN download stories from API and save a successful response locally`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val refreshUsecase = spy(usecases.RefreshPocketStories(testContext))
        val successfulResponse = getFailedPocketStories()

        doReturn(successfulResponse).`when`(pocketEndoint).getRecommendedStories()

        val result = refreshUsecase.invoke()

        assertFalse(result)
        verify(pocketEndoint).getRecommendedStories()
        verify(pocketRepo, never()).addAllPocketApiStories(any())
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN GetPocketStories is called THEN delegate the repository to return locally stored stories`() =
        runTest {
            val getStoriesUsecase = spy(usecases.GetPocketStories(testContext))

            doReturn(emptyList<PocketRecommendedStory>()).`when`(pocketRepo)
                .getPocketRecommendedStories()
            var result = getStoriesUsecase.invoke()
            verify(pocketRepo).getPocketRecommendedStories()
            assertTrue(result.isEmpty())

            val stories = listOf(PocketTestResources.clientExpectedPocketStory)
            doReturn(stories).`when`(pocketRepo).getPocketRecommendedStories()
            result = getStoriesUsecase.invoke()
            // getPocketRecommendedStories() should've been called 2 times. Once in the above check, once now.
            verify(pocketRepo, times(2)).getPocketRecommendedStories()
            assertEquals(result, stories)
        }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN UpdateStoriesTimesShown is called THEN delegate the repository to update the stories shown`() = runTest {
        val updateStoriesTimesShown = usecases.UpdateStoriesTimesShown(testContext)
        val updatedStories: List<PocketRecommendedStory> = mock()

        updateStoriesTimesShown.invoke(updatedStories)

        verify(pocketRepo).updateShownPocketRecommendedStories(updatedStories)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN GetSponsoredStories is called THEN return the stories from provider`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(UUID.randomUUID(), "test")
        )
        val successfulResponse = getSuccessfulSponsoredStories()
        doReturn(successfulResponse).`when`(spocsProvider).getSponsoredStories()
        val expectedSponsoredStories = (successfulResponse as PocketResponse.Success).data.map {
            PocketSponsoredStory(
                title = it.title,
                url = it.url,
                imageUrl = it.imageSrc,
                sponsor = it.sponsor,
                shim = PocketSponsoredStoryShim(
                    click = it.shim.click,
                    impression = it.shim.impression
                )
            )
        }

        val result = sponsoredStoriesUsecase.invoke()

        verify(spocsProvider).getSponsoredStories()
        assertEquals(expectedSponsoredStories, result)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN GetSponsoredStories is called THEN return return an empty list if the provider fails`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = PocketResponse.Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).getSponsoredStories()
        val expectedSponsoredStories = emptyList<List<PocketSponsoredStory>>()

        val result = sponsoredStoriesUsecase.invoke()

        verify(spocsProvider).getSponsoredStories()
        assertEquals(expectedSponsoredStories, result)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN DeleteUserProfile is called THEN return true if profile deletion was successful`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteUserProfile(UUID.randomUUID(), "test")
        )
        val successfulResponse = PocketResponse.Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        assertTrue(result)
    }

    @Test
    fun `GIVEN PocketStoriesUseCases WHEN DeleteUserProfile is called THEN return false if profile deletion was not successful`() = runTest {
        PocketStoriesUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteUserProfile(UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = PocketResponse.Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        assertFalse(result)
    }

    private fun getSuccessfulPocketStories() =
        PocketResponse.wrap(PocketTestResources.apiExpectedPocketStoriesRecommendations)

    private fun getSuccessfulSponsoredStories() =
        PocketResponse.wrap(PocketTestResources.apiExpectedPocketSpocs)

    private fun getFailedPocketStories() = PocketResponse.wrap(null)
}
