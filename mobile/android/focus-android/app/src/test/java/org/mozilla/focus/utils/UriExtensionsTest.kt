/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.net.Uri
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class UriExtensionsTest {
    @Test
    fun testTruncatedHost() {
        assertTruncatedHost("mozilla.org", "https://www.mozilla.org")
        assertTruncatedHost("wikipedia.org", "https://en.m.wikipedia.org/wiki/")
        assertTruncatedHost("", "tel://")
        assertTruncatedHost("example.org", "http://example.org")
    }

    @Test
    fun testTruncatedPath() {
        assertTruncatedPath("", "https://www.mozilla.org")
        assertTruncatedPath("", "https://www.mozilla.org/")
        assertTruncatedPath("", "https://www.mozilla.org///")

        assertTruncatedPath("/en-US/firefox", "https://www.mozilla.org/en-US/firefox/")
        assertTruncatedPath("/en-US/…/fast", "https://www.mozilla.org/en-US/firefox/features/fast/")

        assertTruncatedPath("/space", "https://www.theverge.com/space")
        assertTruncatedPath("/2017/…/nasa-hi-seas-mars-analogue-mission-hawaii-mauna-loa",
                "https://www.theverge.com/2017/9/24/16356876/nasa-hi-seas-mars-analogue-mission-hawaii-mauna-loa")

    }

    private fun assertTruncatedHost(expectedTruncatedPath: String, url: String) {
        assertEquals("truncatedHost($url)",
                expectedTruncatedPath,
                Uri.parse(url).truncatedHost())
    }

    private fun assertTruncatedPath(expectedTruncatedPath: String, url: String) {
        assertEquals("truncatedPath($url)",
                expectedTruncatedPath,
                Uri.parse(url).truncatedPath())
    }
}
