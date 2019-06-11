/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class PocketListenURLsTest {

    private lateinit var urls: PocketListenURLs

    @Before
    fun setUp() {
        urls = PocketListenURLs()
    }

    @Test
    fun `WHEN getting the articleservice url THEN the last path segment is for articleservice`() {
        assertEquals("articleservice", urls.articleService.lastPathSegment)
    }
}
