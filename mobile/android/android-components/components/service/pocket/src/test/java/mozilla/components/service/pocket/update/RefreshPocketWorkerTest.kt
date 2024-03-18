/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.service.pocket.GlobalDependencyProvider
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.stories.PocketStoriesUseCases
import mozilla.components.service.pocket.stories.PocketStoriesUseCases.RefreshPocketStories
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import kotlin.reflect.KVisibility

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class RefreshPocketWorkerTest {

    @get:Rule
    val mainCoroutineRule = MainCoroutineRule()

    @Test
    fun `GIVEN a RefreshPocketWorker THEN its visibility is internal`() {
        assertClassVisibility(RefreshPocketWorker::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a RefreshPocketWorker WHEN stories are refreshed successfully THEN return success`() = runTestOnMain {
        val useCases: PocketStoriesUseCases = mock()
        val refreshStoriesUseCase: RefreshPocketStories = mock()
        doReturn(true).`when`(refreshStoriesUseCase).invoke()
        doReturn(refreshStoriesUseCase).`when`(useCases).refreshStories
        GlobalDependencyProvider.RecommendedStories.initialize(useCases)
        val worker = TestListenableWorkerBuilder<RefreshPocketWorker>(testContext).build()

        val result = worker.startWork().await()
        assertEquals(ListenableWorker.Result.success(), result)
    }

    @Test
    fun `GIVEN a RefreshPocketWorker WHEN stories are could not be refreshed THEN work should be retried`() = runTestOnMain {
        val useCases: PocketStoriesUseCases = mock()
        val refreshStoriesUseCase: RefreshPocketStories = mock()
        doReturn(false).`when`(refreshStoriesUseCase).invoke()
        doReturn(refreshStoriesUseCase).`when`(useCases).refreshStories
        GlobalDependencyProvider.RecommendedStories.initialize(useCases)
        val worker = TestListenableWorkerBuilder<RefreshPocketWorker>(testContext).build()

        val result = worker.startWork().await()
        assertEquals(ListenableWorker.Result.retry(), result)
    }
}
