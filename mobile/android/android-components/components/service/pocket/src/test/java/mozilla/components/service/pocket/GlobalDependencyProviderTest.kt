/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.service.pocket.spocs.SpocsUseCases
import mozilla.components.service.pocket.stories.PocketStoriesUseCases
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test

class GlobalDependencyProviderTest {
    @Test
    fun `GIVEN RecommendedStories WHEN initializing THEN store the provided arguments`() {
        val useCases: PocketStoriesUseCases = mock()

        GlobalDependencyProvider.RecommendedStories.initialize(useCases)

        assertSame(useCases, GlobalDependencyProvider.RecommendedStories.useCases)
    }

    @Test
    fun `GIVEN RecommendedStories WHEN resetting THEN clear all current state`() {
        GlobalDependencyProvider.RecommendedStories.initialize(mock())

        GlobalDependencyProvider.RecommendedStories.reset()

        assertNull(GlobalDependencyProvider.RecommendedStories.useCases)
    }

    @Test
    fun `GIVEN SponsoredStories WHEN initializing THEN store the provided arguments`() {
        val useCases: SpocsUseCases = mock()

        GlobalDependencyProvider.SponsoredStories.initialize(useCases)

        assertSame(useCases, GlobalDependencyProvider.SponsoredStories.useCases)
    }

    @Test
    fun `GIVEN SponsoredStories WHEN resetting THEN clear all current state`() {
        GlobalDependencyProvider.SponsoredStories.initialize(mock())

        GlobalDependencyProvider.SponsoredStories.reset()

        assertNull(GlobalDependencyProvider.SponsoredStories.useCases)
    }
}
