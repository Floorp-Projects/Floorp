/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.net.Uri
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class UriTest {
    @Test
    fun testTruncatedHostWithCommonUrls() {
        assertTruncatedHost("mozilla.org", "https://www.mozilla.org")
        assertTruncatedHost("wikipedia.org", "https://en.m.wikipedia.org/wiki/")
        assertTruncatedHost("example.org", "http://example.org")
        assertTruncatedHost("youtube.com", "https://www.youtube.com/watch?v=oHg5SJYRHA0")
        assertTruncatedHost("facebook.com", "https://www.facebook.com/Firefox/")
        assertTruncatedHost("yahoo.com", "https://de.search.yahoo.com/search?p=mozilla&fr=yfp-t&fp=1&toggle=1&cop=mss&ei=UTF-8")
        assertTruncatedHost("amazon.co.uk", "https://www.amazon.co.uk/Doctor-Who-10-Part-DVD/dp/B06XCMVY1H")
    }

    @Test
    fun testTruncatedHostWithEmptyHost() {
        assertTruncatedHost("", "tel://")
    }

    @Test
    fun testTruncatedPathWithEmptySegments() {
        assertTruncatedPath("", "https://www.mozilla.org")
        assertTruncatedPath("", "https://www.mozilla.org/")
        assertTruncatedPath("", "https://www.mozilla.org///")
    }

    @Test
    fun testTrunactedPathWithOneSegment() {
        assertTruncatedPath("/space", "https://www.theverge.com/space")
    }

    @Test
    fun testTruncatedPathWithTwoSegments() {
        assertTruncatedPath("/en-US/firefox", "https://www.mozilla.org/en-US/firefox/")
        assertTruncatedPath("/mozilla-mobile/focus-android", "https://github.com/mozilla-mobile/focus-android")
    }

    @Test
    fun testTruncatedPathWithMultipleSegments() {
        assertTruncatedPath("/en-US/…/fast", "https://www.mozilla.org/en-US/firefox/features/fast/")

        assertTruncatedPath(
            "/2017/…/nasa-hi-seas-mars-analogue-mission-hawaii-mauna-loa",
            "https://www.theverge.com/2017/9/24/16356876/nasa-hi-seas-mars-analogue-mission-hawaii-mauna-loa"
        )
    }

    @Test
    fun testTruncatedPathWithMultipleSegmentsAndFragment() {
        assertTruncatedPath(
            "/@bfrancis/the-story-of-firefox-os-cb5bf796e8fb",
            "https://medium.com/@bfrancis/the-story-of-firefox-os-cb5bf796e8fb#931a"
        )
    }

    private fun assertTruncatedHost(expectedTruncatedPath: String, url: String) {
        assertEquals(
            "truncatedHost($url)",
            expectedTruncatedPath,
            Uri.parse(url).truncatedHost()
        )
    }

    private fun assertTruncatedPath(expectedTruncatedPath: String, url: String) {
        assertEquals(
            "truncatedPath($url)",
            expectedTruncatedPath,
            Uri.parse(url).truncatedPath()
        )
    }
}
