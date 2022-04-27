/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.PocketSponsoredStory
import mozilla.components.service.pocket.PocketSponsoredStoryShim
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@OptIn(ExperimentalCoroutinesApi::class) // for runTest
@RunWith(AndroidJUnit4::class)
class SpocsUseCasesTest {
    private val usecases = spy(SpocsUseCases())
    private val spocsProvider: SpocsEndpoint = mock()

    @Before
    fun setup() {
        doReturn(spocsProvider).`when`(usecases).getSpocsProvider(any(), any(), any())
    }
    @After
    fun cleanup() {
        SpocsUseCases.reset()
    }

    @Test
    fun `GIVEN a SpocsUseCases THEN its visibility is internal`() {
        assertClassVisibility(SpocsUseCases::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a GetSponsoredStories THEN its visibility is internal`() {
        assertClassVisibility(SpocsUseCases.GetSponsoredStories::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a DeleteProfile THEN its visibility is internal`() {
        assertClassVisibility(SpocsUseCases.DeleteProfile::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return the stories from provider`() = runTest {
        SpocsUseCases.initialize(mock())
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(UUID.randomUUID(), "test")
        )
        val successfulResponse = getSuccessfulSponsoredStories()
        doReturn(successfulResponse).`when`(spocsProvider).getSponsoredStories()
        val expectedSponsoredStories = (successfulResponse as Success).data.map {
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
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return return an empty list if the provider fails`() = runTest {
        SpocsUseCases.initialize(mock())
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).getSponsoredStories()
        val expectedSponsoredStories = emptyList<List<PocketSponsoredStory>>()

        val result = sponsoredStoriesUsecase.invoke()

        verify(spocsProvider).getSponsoredStories()
        assertEquals(expectedSponsoredStories, result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return true if profile deletion was successful`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(UUID.randomUUID(), "test")
        )
        val successfulResponse = Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        Assert.assertTrue(result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return false if profile deletion was not successful`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        Assert.assertFalse(result)
    }

    private fun getSuccessfulSponsoredStories() =
        PocketResponse.wrap(PocketTestResources.apiExpectedPocketSpocs)
}
