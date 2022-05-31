/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.ListenableWorker.Result
import androidx.work.await
import androidx.work.testing.TestListenableWorkerBuilder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.service.pocket.GlobalDependencyProvider
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.spocs.SpocsUseCases
import mozilla.components.service.pocket.spocs.SpocsUseCases.DeleteProfile
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import kotlin.reflect.KVisibility.INTERNAL

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class DeleteSpocsProfileWorkerTest {
    @get:Rule
    val mainCoroutineRule = MainCoroutineRule()

    @Test
    fun `GIVEN a DeleteSpocsProfileWorker THEN its visibility is internal`() {
        assertClassVisibility(RefreshSpocsWorker::class, INTERNAL)
    }

    @Test
    fun `GIVEN a DeleteSpocsProfileWorker WHEN profile deletion is successful THEN return success`() = runTestOnMain {
        val useCases: SpocsUseCases = mock()
        val deleteProfileUseCase: DeleteProfile = mock()
        doReturn(true).`when`(deleteProfileUseCase).invoke()
        doReturn(deleteProfileUseCase).`when`(useCases).deleteProfile
        GlobalDependencyProvider.SponsoredStories.initialize(useCases)
        val worker = TestListenableWorkerBuilder<DeleteSpocsProfileWorker>(testContext).build()

        val result = worker.startWork().await()

        assertEquals(Result.success(), result)
    }

    @Test
    fun `GIVEN a DeleteSpocsProfileWorker WHEN profile deletion fails THEN work should be retried`() = runTestOnMain {
        val useCases: SpocsUseCases = mock()
        val deleteProfileUseCase: DeleteProfile = mock()
        doReturn(false).`when`(deleteProfileUseCase).invoke()
        doReturn(deleteProfileUseCase).`when`(useCases).deleteProfile
        GlobalDependencyProvider.SponsoredStories.initialize(useCases)
        val worker = TestListenableWorkerBuilder<DeleteSpocsProfileWorker>(testContext).build()

        val result = worker.startWork().await()

        assertEquals(Result.retry(), result)
    }
}
