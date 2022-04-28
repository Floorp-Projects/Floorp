/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import mozilla.components.service.pocket.spocs.SpocsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketStoriesServiceTest {
    private val service = PocketStoriesService(testContext, PocketStoriesConfig(mock())).also {
        it.storiesRefreshScheduler = mock()
        it.getStoriesUsecase = mock()
    }

    @Test
    fun `GIVEN PocketStoriesService THEN it should be publicly available`() {
        assertConstructorsVisibility(PocketStoriesConfig::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN startPeriodicStoriesRefresh THEN scheduler#schedulePeriodicRefreshes should be called`() {
        service.startPeriodicStoriesRefresh()

        verify(service.storiesRefreshScheduler).schedulePeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN stopPeriodicStoriesRefresh THEN scheduler#stopPeriodicRefreshes should be called`() {
        service.stopPeriodicStoriesRefresh()

        verify(service.storiesRefreshScheduler).stopPeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with a valid profile WHEN called to start periodic refreshes THEN delegate SpocsRefreshScheduler`() {
        val client: Client = mock()
        val profileId = UUID.randomUUID()
        val appId = "test"
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = client,
                profile = Profile(
                    profileId = profileId,
                    appId = appId
                )
            )
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.startPeriodicSponsoredStoriesRefresh()

        verify(service.spocsRefreshscheduler).schedulePeriodicRefreshes(any())
        assertSame(client, SpocsUseCases.fetchClient)
        assertEquals(profileId, SpocsUseCases.profileId)
        assertEquals(appId, SpocsUseCases.appId)
    }

    @Test
    fun `GIVEN PocketStoriesService is initialized with an invalid profile WHEN called to start periodic refreshes THEN don't delegate SpocsRefreshScheduler`() {
        val service = PocketStoriesService(
            context = testContext,
            pocketStoriesConfig = PocketStoriesConfig(
                client = mock(),
                profile = null
            )
        ).apply {
            spocsRefreshscheduler = mock()
        }

        service.startPeriodicSponsoredStoriesRefresh()

        verify(service.spocsRefreshscheduler, never()).schedulePeriodicRefreshes(any())
    }

    @Test
    fun `GIVEN PocketStoriesService WHEN called to stop periodic refreshes THEN stop refreshing through SpocsRefreshScheduler`() {
        // Mock periodic refreshes were started previously and profile details were set.
        // Now they will have to be cleaned.
        SpocsUseCases.apply {
            fetchClient = mock()
            profileId = UUID.randomUUID()
            appId = "test"
        }
        service.spocsRefreshscheduler = mock()

        service.stopPeriodicSponsoredStoriesRefresh()

        verify(service.spocsRefreshscheduler).stopPeriodicRefreshes(any())
        assertNull(SpocsUseCases.fetchClient)
        assertNull(SpocsUseCases.profileId)
        assertNull(SpocsUseCases.appId)
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `GIVEN PocketStoriesService WHEN getStories THEN getStoriesUsecase should return`() = runTest {
        val stories = listOf(mock<PocketRecommendedStory>())
        whenever(service.getStoriesUsecase.invoke()).thenReturn(stories)

        val result = service.getStories()

        assertEquals(stories, result)
    }
}
