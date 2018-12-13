/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class DomainMatcherTest {
    @Test
    fun `should perform basic domain matching for a given query`() {
        assertNull(segmentAwareDomainMatch("moz", listOf()))

        val urls = listOf("http://www.mozilla.org", "http://firefox.com", "https://en.wikipedia.org/wiki/Mozilla", "about:config")
        assertEquals(
                DomainMatch("http://www.mozilla.org", "mozilla.org"),
                segmentAwareDomainMatch("moz", urls)
        )
        assertEquals(
                DomainMatch("http://www.mozilla.org", "www.mozilla.org"),
                segmentAwareDomainMatch("www.moz", urls)
        )
        assertEquals(
                DomainMatch("https://en.wikipedia.org/wiki/Mozilla", "en.wikipedia.org/wiki/Mozilla"),
                segmentAwareDomainMatch("en", urls)
        )
        assertEquals(
                DomainMatch("http://firefox.com", "firefox.com"),
                segmentAwareDomainMatch("fire", urls)
        )
        assertEquals(
                DomainMatch("http://www.mozilla.org", "http://www.mozilla.org"),
                segmentAwareDomainMatch("http://www.m", urls)
        )

        assertNull(segmentAwareDomainMatch("nomatch", urls))
    }
}
