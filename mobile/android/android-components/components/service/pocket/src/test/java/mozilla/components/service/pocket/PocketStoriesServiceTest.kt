/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import mozilla.components.service.pocket.spocs.SpocsUseCases
import mozilla.components.service.pocket.spocs.SpocsUseCases.GetSponsoredStories
import mozilla.components.service.pocket.spocs.SpocsUseCases.RecordImpression
import mozilla.components.service.pocket.stories.PocketStoriesUseCases
import mozilla.components.service.pocket.stories.PocketStoriesUseCases.GetPocketStories
import mozilla.components.service.pocket.stories.PocketStoriesUseCases.UpdateStoriesTimesShown
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class PocketStoriesServiceTest {
    private val storiesUseCases: PocketStoriesUseCases = mock()
    private val spocsUseCases: SpocsUseCases = mock()
    private val service = PocketStoriesService(testContext, PocketStoriesConfig(mock())).also {
        it.storiesRefreshScheduler = mock()
        it.spocsRefreshscheduler = mock()
        it.storiesUseCases = storiesUseCases
        it.spocsUseCases = spocsUseCases
    }

    @After
    fun teardown() {
        GlobalDependencyProvider.SponsoredStories.reset()
        GlobalDependencyProvider.RecommendedStories.reset()
    }

    @Test
    fun `GIVEN PocketStoriesService THEN it should be publicly available`() {
        assertConstructorsVisibility(PocketStoriesConfig::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN startPeriodicStoriesRefresh THEN persist dependencies and schedule stories refresh`() {
        service.startPeriodicStoriesRefresh()

        assertNotNull(GlobalDependencyProvider.RecommendedStories.useCases)
        verify(service.storiesRefreshScheduler).schedulePeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN stopPeriodicStoriesRefresh THEN stop refreshing stories and clear dependencies`() {
        service.stopPeriodicStoriesRefresh()

        verify(service.storiesRefreshScheduler).stopPeriodicRefreshes(any())
        assertNull(GlobalDependencyProvider.RecommendedStories.useCases)
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with a valid profile WHEN called to start periodic refreshes THEN persist dependencies, cancel profile deletion and schedule stories refresh`() {
        val client: Client = mock()
        val profileId = UUID.randomUUID()
        val appId = "test"
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = client,
                profile = Profile(
                    profileId = profileId,
                    appId = appId,
                ),
            ),
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.startPeriodicSponsoredStoriesRefresh()

        assertNotNull(GlobalDependencyProvider.SponsoredStories.useCases)
        verify(service.spocsRefreshscheduler).stopProfileDeletion(any())
        verify(service.spocsRefreshscheduler).schedulePeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with an invalid profile WHEN called to start periodic refreshes THEN don't schedule periodic refreshes and don't persist dependencies`() {
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = mock(),
                profile = null,
            ),
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.startPeriodicSponsoredStoriesRefresh()

        verify(service.spocsRefreshscheduler, never()).schedulePeriodicRefreshes(any())
        assertNull(GlobalDependencyProvider.SponsoredStories.useCases)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN called to stop periodic refreshes THEN stop refreshing stories`() {
        // Mock periodic refreshes were started previously and profile details were set.
        // Now they will have to be cleaned.
        GlobalDependencyProvider.SponsoredStories.initialize(mock())
        service.spocsRefreshscheduler = mock()

        service.stopPeriodicSponsoredStoriesRefresh()

        verify(service.spocsRefreshscheduler).stopPeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN getStories THEN stories useCases should return`() = runTest {
        val stories = listOf(mock<PocketRecommendedStory>())
        val getStoriesUseCase: GetPocketStories = mock()
        doReturn(stories).`when`(getStoriesUseCase).invoke()
        doReturn(getStoriesUseCase).`when`(storiesUseCases).getStories

        val result = service.getStories()

        assertEquals(stories, result)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN updateStoriesTimesShown THEN delegate to spocs useCases`() = runTest {
        val updateTimesShownUseCase: UpdateStoriesTimesShown = mock()
        doReturn(updateTimesShownUseCase).`when`(storiesUseCases).updateTimesShown
        val stories = listOf(mock<PocketRecommendedStory>())

        service.updateStoriesTimesShown(stories)

        verify(updateTimesShownUseCase).invoke(stories)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN getSponsoredStories THEN delegate to spocs useCases`() = runTest {
        val noProfileResponse = service.getSponsoredStories()
        assertTrue(noProfileResponse.isEmpty())

        val stories = listOf(mock<PocketSponsoredStory>())
        val getStoriesUseCase: GetSponsoredStories = mock()
        doReturn(stories).`when`(getStoriesUseCase).invoke()
        doReturn(getStoriesUseCase).`when`(spocsUseCases).getStories
        val existingProfileResponse = service.getSponsoredStories()
        assertEquals(stories, existingProfileResponse)
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with a valid profile WHEN called to delete profile THEN persist dependencies, cancel stories refresh and schedule profile deletion`() {
        val client: Client = mock()
        val profileId = UUID.randomUUID()
        val appId = "test"
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = client,
                profile = Profile(
                    profileId = profileId,
                    appId = appId,
                ),
            ),
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.deleteProfile()

        assertNotNull(GlobalDependencyProvider.SponsoredStories.useCases)
        verify(service.spocsRefreshscheduler).stopPeriodicRefreshes(any())
        verify(service.spocsRefreshscheduler).scheduleProfileDeletion(any())
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with an invalid profile WHEN called to delete profile THEN don't schedule profile deletion and don't persist dependencies`() {
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = mock(),
                profile = null,
            ),
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.deleteProfile()

        verify(service.spocsRefreshscheduler, never()).scheduleProfileDeletion(any())
        assertNull(GlobalDependencyProvider.SponsoredStories.useCases)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN recordStoriesImpressions THEN delegate to spocs useCases`() = runTest {
        val recordImpressionsUseCase: RecordImpression = mock()
        doReturn(recordImpressionsUseCase).`when`(spocsUseCases).recordImpression
        val storiesIds = listOf(22, 33)

        service.recordStoriesImpressions(storiesIds)

        verify(recordImpressionsUseCase).invoke(storiesIds)
    }
}
