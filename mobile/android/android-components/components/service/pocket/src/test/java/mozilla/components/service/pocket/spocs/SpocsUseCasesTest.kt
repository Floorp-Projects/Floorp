/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.spocs.api.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@OptIn(ExperimentalCoroutinesApi::class) // for runTest
@RunWith(AndroidJUnit4::class)
class SpocsUseCasesTest {
    private val usecases = spy(SpocsUseCases())
    private val spocsProvider: SpocsEndpoint = mock()
    private val spocsRepo: SpocsRepository = mock()

    @Before
    fun setup() {
        doReturn(spocsProvider).`when`(usecases).getSpocsProvider(any(), any(), any())
        doReturn(spocsRepo).`when`(usecases).getSpocsRepository(any())
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
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return the stories from repository`() = runTest {
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(testContext, UUID.randomUUID(), "test")
        )
        val stories = listOf(PocketTestResources.clientExpectedPocketStory)
        doReturn(stories).`when`(spocsRepo).getAllSpocs()

        val result = sponsoredStoriesUsecase.invoke()

        verify(spocsRepo).getAllSpocs()
        assertEquals(result, stories)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return return an empty list if none are available in the repository`() = runTest {
        val sponsoredStoriesUsecase = spy(
            usecases.GetSponsoredStories(testContext, UUID.randomUUID(), "test")
        )
        doReturn(emptyList<PocketRecommendedStory>()).`when`(spocsRepo).getAllSpocs()

        val result = sponsoredStoriesUsecase.invoke()

        verify(spocsRepo).getAllSpocs()
        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return true if profile deletion was successful`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(testContext, UUID.randomUUID(), "test")
        )
        val successfulResponse = Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        assertTrue(result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return false if profile deletion was not successful`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(testContext, UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUsecase.invoke()

        verify(spocsProvider).deleteProfile()
        assertFalse(result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN profile deletion is succesfull THEN delete all locally persisted spocs`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(testContext, UUID.randomUUID(), "test")
        )
        val successfulResponse = Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        deleteProfileUsecase.invoke()

        verify(spocsRepo).deleteAllSpocs()
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN profile deletion is not succesfull THEN keep all locally persisted spocs`() = runTest {
        SpocsUseCases.initialize(mock())
        val deleteProfileUsecase = spy(
            usecases.DeleteProfile(testContext, UUID.randomUUID(), "test")
        )
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        deleteProfileUsecase.invoke()

        verify(spocsRepo, never()).deleteAllSpocs()
    }
}
