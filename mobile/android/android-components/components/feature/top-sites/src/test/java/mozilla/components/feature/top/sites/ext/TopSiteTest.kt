/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.ext

import mozilla.components.feature.top.sites.TopSite
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TopSiteTest {

    @Test
    fun hasUrl() {
        val topSites = listOf(
            TopSite.Frecent(
                id = 1,
                title = "Mozilla",
                url = "https://mozilla.com",
                createdAt = 1,
            ),
        )

        assertTrue(topSites.hasUrl("https://mozilla.com"))
        assertTrue(topSites.hasUrl("https://www.mozilla.com"))
        assertTrue(topSites.hasUrl("http://mozilla.com"))
        assertTrue(topSites.hasUrl("http://www.mozilla.com"))
        assertTrue(topSites.hasUrl("mozilla.com"))
        assertTrue(topSites.hasUrl("https://mozilla.com/"))

        assertFalse(topSites.hasUrl("https://m.mozilla.com"))
        assertFalse(topSites.hasUrl("https://mozilla.com/path"))
        assertFalse(topSites.hasUrl("https://firefox.com"))
        assertFalse(topSites.hasUrl("https://mozilla.com/path/is/long"))
        assertFalse(topSites.hasUrl("https://mozilla.com/path#anchor"))
    }
}
