/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.spocs.SpocsUseCases.RefreshSponsoredStories
import mozilla.components.service.pocket.spocs.api.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
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
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@OptIn(ExperimentalCoroutinesApi::class) // for runTest
@RunWith(AndroidJUnit4::class)
class SpocsUseCasesTest {
    private val fetchClient: Client = mock()
    private val profileId = UUID.randomUUID()
    private val appId = "test"
    private val useCases = spy(SpocsUseCases(testContext, fetchClient, profileId, appId))
    private val spocsProvider: SpocsEndpoint = mock()
    private val spocsRepo: SpocsRepository = mock()

    @Before
    fun setup() {
        doReturn(spocsProvider).`when`(useCases).getSpocsProvider(any(), any(), any())
        doReturn(spocsRepo).`when`(useCases).getSpocsRepository(any())
    }

    @Test
    fun `GIVEN a SpocsUseCases THEN its visibility is internal`() {
        assertClassVisibility(SpocsUseCases::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a RefreshSponsoredStories THEN its visibility is internal`() {
        assertClassVisibility(RefreshSponsoredStories::class, KVisibility.INTERNAL)
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
    fun `GIVEN SpocsUseCases WHEN RefreshSponsoredStories is constructed THEN use the same parameters`() {
        val refreshUseCase = useCases.refreshStories

        assertSame(testContext, refreshUseCase.appContext)
        assertSame(fetchClient, refreshUseCase.fetchClient)
        assertSame(profileId, refreshUseCase.profileId)
        assertSame(appId, refreshUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN RefreshSponsoredStories is constructed separately THEN default to use the same parameters`() {
        val refreshUseCase = useCases.RefreshSponsoredStories()

        assertSame(testContext, refreshUseCase.appContext)
        assertSame(fetchClient, refreshUseCase.fetchClient)
        assertSame(profileId, refreshUseCase.profileId)
        assertSame(appId, refreshUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN RefreshSponsoredStories is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()
        val fetchClient2: Client = mock()
        val profileId2 = UUID.randomUUID()
        val appId2 = "test"

        val refreshUseCase = useCases.RefreshSponsoredStories(context2, fetchClient2, profileId2, appId2)

        assertSame(context2, refreshUseCase.appContext)
        assertSame(fetchClient2, refreshUseCase.fetchClient)
        assertSame(profileId2, refreshUseCase.profileId)
        assertSame(appId2, refreshUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN RefreshSponsoredStories is called THEN download stories from API and return early if unsuccessful response`() = runTest {
        val refreshUseCase = useCases.RefreshSponsoredStories()
        val unsuccessfulResponse = getFailedSponsoredStories()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).getSponsoredStories()

        val result = refreshUseCase.invoke()

        assertFalse(result)
        verify(spocsProvider).getSponsoredStories()
        verify(spocsRepo, never()).addSpocs(any())
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN RefreshSponsoredStories is called THEN download stories from API and save a successful response locally`() = runTest {
        val refreshUseCase = useCases.RefreshSponsoredStories()
        val successfulResponse = getSuccessfulSponsoredStories()
        doReturn(successfulResponse).`when`(spocsProvider).getSponsoredStories()

        val result = refreshUseCase.invoke()

        assertTrue(result)
        verify(spocsProvider).getSponsoredStories()
        verify(spocsRepo).addSpocs((successfulResponse as Success).data)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is constructed THEN use the same parameters`() {
        val sponsoredStoriesUseCase = useCases.getStories

        assertSame(testContext, sponsoredStoriesUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN GetSponsoredStories is constructed separately THEN default to use the same parameters`() {
        val sponsoredStoriesUseCase = useCases.GetSponsoredStories()

        assertSame(testContext, sponsoredStoriesUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN GetSponsoredStories is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()

        val sponsoredStoriesUseCase = useCases.GetSponsoredStories(context2)

        assertSame(context2, sponsoredStoriesUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return the stories from repository`() = runTest {
        val sponsoredStoriesUseCase = useCases.GetSponsoredStories()
        val stories = listOf(PocketTestResources.clientExpectedPocketStory)
        doReturn(stories).`when`(spocsRepo).getAllSpocs()

        val result = sponsoredStoriesUseCase.invoke()

        verify(spocsRepo).getAllSpocs()
        assertEquals(result, stories)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN GetSponsoredStories is called THEN return return an empty list if none are available in the repository`() = runTest {
        val sponsoredStoriesUseCase = useCases.GetSponsoredStories()
        doReturn(emptyList<PocketRecommendedStory>()).`when`(spocsRepo).getAllSpocs()

        val result = sponsoredStoriesUseCase.invoke()

        verify(spocsRepo).getAllSpocs()
        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN RecordImpression is constructed THEN use the same parameters`() {
        val recordImpressionsUseCase = useCases.getStories

        assertSame(testContext, recordImpressionsUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN RecordImpression is constructed separately THEN default to use the same parameters`() {
        val recordImpressionsUseCase = useCases.RecordImpression()

        assertSame(testContext, recordImpressionsUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN RecordImpression is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()

        val recordImpressionsUseCase = useCases.RecordImpression(context2)

        assertSame(context2, recordImpressionsUseCase.context)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN RecordImpression is called THEN record impressions in database`() = runTest {
        val recordImpressionsUseCase = useCases.RecordImpression()
        val storiesIds = listOf(5, 55, 4321)
        val spocsIdsCaptor = argumentCaptor<List<Int>>()

        recordImpressionsUseCase(storiesIds)

        verify(spocsRepo).recordImpressions(spocsIdsCaptor.capture())
        assertEquals(3, spocsIdsCaptor.value.size)
        assertEquals(storiesIds[0], spocsIdsCaptor.value[0])
        assertEquals(storiesIds[1], spocsIdsCaptor.value[1])
        assertEquals(storiesIds[2], spocsIdsCaptor.value[2])
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is constructed THEN use the same parameters`() {
        val deleteProfileUseCase = useCases.deleteProfile

        assertSame(testContext, deleteProfileUseCase.context)
        assertSame(fetchClient, deleteProfileUseCase.fetchClient)
        assertSame(profileId, deleteProfileUseCase.profileId)
        assertSame(appId, deleteProfileUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN DeleteProfile is constructed separately THEN default to use the same parameters`() {
        val deleteProfileUseCase = useCases.DeleteProfile()

        assertSame(testContext, deleteProfileUseCase.context)
        assertSame(fetchClient, deleteProfileUseCase.fetchClient)
        assertSame(profileId, deleteProfileUseCase.profileId)
        assertSame(appId, deleteProfileUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases constructed WHEN DeleteProfile is constructed separately THEN allow using different parameters`() {
        val context2: Context = mock()
        val fetchClient2: Client = mock()
        val profileId2 = UUID.randomUUID()
        val appId2 = "test"

        val deleteProfileUseCase = useCases.DeleteProfile(context2, fetchClient2, profileId2, appId2)

        assertSame(context2, deleteProfileUseCase.context)
        assertSame(fetchClient2, deleteProfileUseCase.fetchClient)
        assertSame(profileId2, deleteProfileUseCase.profileId)
        assertSame(appId2, deleteProfileUseCase.appId)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return true if profile deletion was successful`() = runTest {
        val deleteProfileUseCase = useCases.DeleteProfile()
        val successfulResponse = Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUseCase.invoke()

        verify(spocsProvider).deleteProfile()
        assertTrue(result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN DeleteProfile is called THEN return false if profile deletion was not successful`() = runTest {
        val deleteProfileUseCase = useCases.DeleteProfile()
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        val result = deleteProfileUseCase.invoke()

        verify(spocsProvider).deleteProfile()
        assertFalse(result)
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN profile deletion is succesfull THEN delete all locally persisted spocs`() = runTest {
        val deleteProfileUseCase = useCases.DeleteProfile()
        val successfulResponse = Success(true)
        doReturn(successfulResponse).`when`(spocsProvider).deleteProfile()

        deleteProfileUseCase.invoke()

        verify(spocsRepo).deleteAllSpocs()
    }

    @Test
    fun `GIVEN SpocsUseCases WHEN profile deletion is not succesfull THEN keep all locally persisted spocs`() = runTest {
        val deleteProfileUseCase = useCases.DeleteProfile()
        val unsuccessfulResponse = Failure<Any>()
        doReturn(unsuccessfulResponse).`when`(spocsProvider).deleteProfile()

        deleteProfileUseCase.invoke()

        verify(spocsRepo, never()).deleteAllSpocs()
    }

    private fun getSuccessfulSponsoredStories() =
        PocketResponse.wrap(PocketTestResources.apiExpectedPocketSpocs)

    private fun getFailedSponsoredStories() = PocketResponse.wrap(null)
}
