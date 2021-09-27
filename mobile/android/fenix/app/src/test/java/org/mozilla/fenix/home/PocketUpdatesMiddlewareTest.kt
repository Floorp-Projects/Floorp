/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStoriesService
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Test

class PocketUpdatesMiddlewareTest {
    @ExperimentalCoroutinesApi
    @Test
    fun `WHEN PocketStoriesShown is dispatched THEN update PocketStoriesService`() {
        val story1 = PocketRecommendedStory("title", "url1", "imageUrl", "publisher", "category", 0, timesShown = 0)
        val story2 = story1.copy("title2", "url2")
        val story3 = story1.copy("title3", "url3")
        val coroutineScope = TestCoroutineScope()
        val pocketService: PocketStoriesService = mockk(relaxed = true)
        val pocketMiddleware = PocketUpdatesMiddleware(coroutineScope, pocketService)
        val homeStore = HomeFragmentStore(
            HomeFragmentState(
                pocketStories = listOf(story1, story2, story3)
            ),
            listOf(pocketMiddleware)
        )

        homeStore.dispatch(HomeFragmentAction.PocketStoriesShown(listOf(story2))).joinBlocking()

        coVerify { pocketService.updateStoriesTimesShown(listOf(story2.copy(timesShown = 1))) }
    }
}
