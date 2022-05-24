/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import mozilla.components.service.pocket.helpers.PocketTestResources
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.reflect.full.memberProperties

class MappersKtTest {
    @Test
    fun `GIVEN a PocketApiStory WHEN toPocketLocalStory is called THEN a one to one mapping is performed and timesShown is set to 0`() {
        val apiStory = PocketTestResources.apiExpectedPocketStoriesRecommendations[0]

        val result = apiStory.toPocketLocalStory()

        assertNotEquals(apiStory::class.memberProperties, result::class.memberProperties)
        assertSame(apiStory.url, result.url)
        assertSame(apiStory.title, result.title)
        assertSame(apiStory.imageUrl, result.imageUrl)
        assertSame(apiStory.publisher, result.publisher)
        assertSame(apiStory.category, result.category)
        assertSame(apiStory.timeToRead, result.timeToRead)
        assertEquals(DEFAULT_TIMES_SHOWN, result.timesShown)
    }

    @Test
    fun `GIVEN a PocketLocalStory WHEN toPocketRecommendedStory is called THEN a one to one mapping is performed`() {
        val localStory = PocketTestResources.dbExpectedPocketStory

        val result = localStory.toPocketRecommendedStory()

        assertNotEquals(localStory::class.memberProperties, result::class.memberProperties)
        assertSame(localStory.url, result.url)
        assertSame(localStory.title, result.title)
        assertSame(localStory.imageUrl, result.imageUrl)
        assertSame(localStory.publisher, result.publisher)
        assertSame(localStory.category, result.category)
        assertSame(localStory.timeToRead, result.timeToRead)
        assertEquals(localStory.timesShown, result.timesShown)
    }

    @Test
    fun `GIVEN a PocketLocalStory with no category WHEN toPocketRecommendedStory is called THEN a one to one mapping is performed and the category is set to general`() {
        val localStory = PocketTestResources.dbExpectedPocketStory.copy(category = "")

        val result = localStory.toPocketRecommendedStory()

        assertNotEquals(localStory::class.memberProperties, result::class.memberProperties)
        assertSame(localStory.url, result.url)
        assertSame(localStory.title, result.title)
        assertSame(localStory.imageUrl, result.imageUrl)
        assertSame(localStory.publisher, result.publisher)
        assertSame(DEFAULT_CATEGORY, result.category)
        assertSame(localStory.timeToRead, result.timeToRead)
        assertEquals(localStory.timesShown, result.timesShown)
    }

    @Test
    fun `GIVEN a PcoketRecommendedStory WHEN toPartialTimeShownUpdate is called THEN only the url and timesShown properties are kept`() {
        val story = PocketTestResources.clientExpectedPocketStory

        val result = story.toPartialTimeShownUpdate()

        assertNotEquals(story::class.memberProperties, result::class.memberProperties)
        assertEquals(2, result::class.memberProperties.size)
        assertSame(story.url, result.url)
        assertSame(story.timesShown, result.timesShown)
    }

    @Test
    fun `GIVEN a spoc downloaded from Internet WHEN it is converted to a local spoc THEN a one to one mapping is made`() {
        val apiStory = PocketTestResources.apiExpectedPocketSpocs[0]

        val result = apiStory.toLocalSpoc()

        assertEquals(apiStory.flightId, result.id)
        assertSame(apiStory.title, result.title)
        assertSame(apiStory.url, result.url)
        assertSame(apiStory.imageSrc, result.imageUrl)
        assertSame(apiStory.sponsor, result.sponsor)
        assertSame(apiStory.shim.click, result.clickShim)
        assertSame(apiStory.shim.impression, result.impressionShim)
        assertEquals(apiStory.priority, result.priority)
        assertEquals(apiStory.caps.lifetimeCount, result.lifetimeCapCount)
        assertEquals(apiStory.caps.flightCount, result.flightCapCount)
        assertEquals(apiStory.caps.flightPeriod, result.flightCapPeriod)
    }

    @Test
    fun `GIVEN a local spoc WHEN it is converted to be exposed to clients THEN a one to one mapping is made`() {
        val localStory = PocketTestResources.dbExpectedPocketSpoc

        val result = localStory.toPocketSponsoredStory()

        assertEquals(localStory.id, result.id)
        assertSame(localStory.title, result.title)
        assertSame(localStory.url, result.url)
        assertSame(localStory.imageUrl, result.imageUrl)
        assertSame(localStory.sponsor, result.sponsor)
        assertSame(localStory.clickShim, result.shim.click)
        assertSame(localStory.impressionShim, result.shim.impression)
        assertEquals(localStory.priority, result.priority)
        assertEquals(localStory.lifetimeCapCount, result.caps.lifetimeCount)
        assertEquals(localStory.flightCapCount, result.caps.flightCount)
        assertEquals(localStory.flightCapPeriod, result.caps.flightPeriod)
        assertTrue(result.caps.currentImpressions.isEmpty())
    }
}
