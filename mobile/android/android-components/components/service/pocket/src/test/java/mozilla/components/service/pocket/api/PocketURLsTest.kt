/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.PocketStoriesConfig
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.reflect.KVisibility

private const val TEST_KEY = "pocketAPIKey"

@RunWith(AndroidJUnit4::class)
class PocketURLsTest {

    private lateinit var urls: PocketURLs

    @Before
    fun setUp() {
        urls = PocketURLs(TEST_KEY)
    }

    @Test
    fun `GIVEN PocketURLs THEN it it's visibility is internal`() {
        assertConstructorsVisibility(PocketStoriesConfig::class, KVisibility.PUBLIC)
    }

    @Test
    fun `WHEN getting the global stories recs url THEN it contains the appropriate query parameters`() {
        val count = 222
        val locale = "EN-us"
        val uri = urls.getLocaleStoriesRecommendations(count, locale)
        assertEquals(TEST_KEY, uri.getQueryParameter(PocketURLs.PARAM_API_KEY))
        assertEquals(count, uri.getQueryParameter(PocketURLs.PARAM_COUNT)!!.toInt())
        assertEquals(locale, uri.getQueryParameter(PocketURLs.PARAM_LOCALE))
    }
}
