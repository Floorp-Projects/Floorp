/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.support.base.worker.Frequency
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import kotlin.reflect.KVisibility

class PocketStoriesConfigTest {
    @Test
    fun `GIVEN a PocketStoriesConfig THEN its visibility is internal`() {
        assertClassVisibility(PocketStoriesConfig::class, KVisibility.PUBLIC)
    }

    @Test
    fun `WHEN instantiating a PocketStoriesConfig THEN frequency has a default value`() {
        val config = PocketStoriesConfig(mock())

        val defaultFrequency = Frequency(DEFAULT_REFRESH_INTERVAL, DEFAULT_REFRESH_TIMEUNIT)
        assertEquals(defaultFrequency.repeatInterval, config.frequency.repeatInterval)
        assertEquals(defaultFrequency.repeatIntervalTimeUnit, config.frequency.repeatIntervalTimeUnit)
    }

    @Test
    fun `WHEN instantiating a PocketStoriesConfig THEN sponsored stories refresh frequency has a default value`() {
        val config = PocketStoriesConfig(mock())

        val defaultFrequency = Frequency(
            DEFAULT_SPONSORED_STORIES_REFRESH_INTERVAL,
            DEFAULT_SPONSORED_STORIES_REFRESH_TIMEUNIT,
        )
        assertEquals(defaultFrequency.repeatInterval, config.sponsoredStoriesRefreshFrequency.repeatInterval)
        assertEquals(defaultFrequency.repeatIntervalTimeUnit, config.sponsoredStoriesRefreshFrequency.repeatIntervalTimeUnit)
    }

    @Test
    fun `WHEN instantiating a PocketStoriesConfig THEN profile is by default null`() {
        val config = PocketStoriesConfig(mock())

        assertNull(config.profile)
    }

    @Test
    fun `GIVEN a Frequency THEN its visibility is internal`() {
        assertClassVisibility(Frequency::class, KVisibility.PUBLIC)
    }

    @Test
    fun `WHEN instantiating a PocketStoriesConfig THEN sponsoredStoriesParams default value is used`() {
        val config = PocketStoriesConfig(mock())

        assertEquals(DEFAULT_SPONSORED_STORIES_SITE_ID, config.sponsoredStoriesParams.siteId)
        assertEquals("", config.sponsoredStoriesParams.country)
        assertEquals("", config.sponsoredStoriesParams.city)
    }
}
