/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

private const val TEST_KEY = "pocketAPIKey"

@RunWith(AndroidJUnit4::class)
class PocketURLsTest {

    private lateinit var urls: PocketURLs

    @Before
    fun setUp() {
        urls = PocketURLs(TEST_KEY)
    }

    @Test
    fun `WHEN getting the global video recs url THEN the last path segment is for global video recs`() {
        assertEquals("global-video-recs", urls.globalVideoRecs.lastPathSegment!!)
    }

    @Test
    fun `WHEN getting the global video recs url THEN it contains the appropriate query parameters`() {
        val uri = urls.globalVideoRecs
        assertUriContainsAPIKeyQueryParams(uri)
        assertEquals(PocketURLs.VALUE_VIDEO_RECS_VERSION, uri.getQueryParameter(PocketURLs.PARAM_VERSION))
        assertEquals(PocketURLs.VALUE_VIDEO_RECS_AUTHORS, uri.getQueryParameter(PocketURLs.PARAM_AUTHORS))
    }

    private fun assertUriContainsAPIKeyQueryParams(uri: Uri) {
        assertEquals(TEST_KEY, uri.getQueryParameter(PocketURLs.PARAM_API_KEY))
    }
}
