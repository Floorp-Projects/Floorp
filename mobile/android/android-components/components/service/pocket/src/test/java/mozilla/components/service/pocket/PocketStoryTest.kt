/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.reflect.KVisibility

class PocketStoryTest {
    @Test
    fun `GIVEN PocketSponsoredStory THEN it should be publicly available`() {
        assertConstructorsVisibility(PocketSponsoredStory::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN PocketSponsoredStoryCaps THEN it should be publicly available`() {
        assertConstructorsVisibility(PocketRecommendedStory::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN PocketRecommendedStory THEN it should be publicly available`() {
        assertConstructorsVisibility(PocketRecommendedStory::class, KVisibility.PUBLIC)
    }

    @Test
    fun `GIVEN a PocketRecommendedStory WHEN it's title is accessed from parent THEN it returns the previously set value`() {
        val pocketRecommendedStory = PocketRecommendedStory(
            title = "testTitle",
            url = "",
            imageUrl = "",
            publisher = "",
            category = "",
            timeToRead = 0,
            timesShown = 0,
        )

        val result = (pocketRecommendedStory as PocketStory).title

        assertEquals("testTitle", result)
    }

    @Test
    fun `GIVEN a PocketRecommendedStory WHEN it's url is accessed from parent THEN it returns the previously set value`() {
        val pocketRecommendedStory = PocketRecommendedStory(
            title = "",
            url = "testUrl",
            imageUrl = "",
            publisher = "",
            category = "",
            timeToRead = 0,
            timesShown = 0,
        )

        val result = (pocketRecommendedStory as PocketStory).url

        assertEquals("testUrl", result)
    }

    @Test
    fun `GIVEN a PocketSponsoredStory WHEN it's title is accessed from parent THEN it returns the previously set value`() {
        val pocketRecommendedStory = PocketSponsoredStory(
            id = 1,
            title = "testTitle",
            url = "",
            imageUrl = "",
            sponsor = "",
            shim = mock(),
            priority = 11,
            caps = mock(),
        )

        val result = (pocketRecommendedStory as PocketStory).title

        assertEquals("testTitle", result)
    }

    @Test
    fun `GIVEN a PocketSponsoredStory WHEN it's url is accessed from parent THEN it returns the previously set value`() {
        val pocketRecommendedStory = PocketSponsoredStory(
            id = 2,
            title = "",
            url = "testUrl",
            imageUrl = "",
            sponsor = "",
            shim = mock(),
            priority = 33,
            caps = mock(),
        )

        val result = (pocketRecommendedStory as PocketStory).url

        assertEquals("testUrl", result)
    }
}
