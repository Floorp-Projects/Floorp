/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.top.sites.TopSite
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
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

    @Test
    fun hasHostOneItem() {
        val topSites = listOf(
            TopSite.Frecent(
                id = 1,
                title = "Amazon",
                url = "https://amazon.com/playstation",
                createdAt = 1,
            ),
        )

        assertTrue(topSites.hasHost("https://amazon.com"))
        assertTrue(topSites.hasHost("https://www.amazon.com"))
        assertTrue(topSites.hasHost("http://amazon.com"))
        assertTrue(topSites.hasHost("http://www.amazon.com"))
        assertTrue(topSites.hasHost("amazon.com"))
        assertTrue(topSites.hasHost("https://amazon.com/"))
        assertTrue(topSites.hasHost("HTTPS://AMAZON.COM/"))
        assertFalse(topSites.hasHost("https://amzn.com/"))
        assertFalse(topSites.hasHost("https://aws.amazon.com/"))
        assertFalse(topSites.hasHost("https://youtube.com/"))
    }

    @Test
    fun hasHostNoItem() {
        val topSites = emptyList<TopSite.Frecent>()

        assertFalse(topSites.hasHost("https://amazon.com"))
        assertFalse(topSites.hasHost("https://www.amazon.com"))
        assertFalse(topSites.hasHost("http://amazon.com"))
        assertFalse(topSites.hasHost("http://www.amazon.com"))
        assertFalse(topSites.hasHost("amazon.com"))
        assertFalse(topSites.hasHost("https://amazon.com/"))
        assertFalse(topSites.hasHost("HTTPS://AMAZON.COM/"))
        assertFalse(topSites.hasHost("https://amzn.com/"))
        assertFalse(topSites.hasHost("https://aws.amazon.com/"))
    }

    @Test
    fun hasHostMultipleItems() {
        val topSites = listOf(
            TopSite.Frecent(
                id = 1,
                title = "Amazon",
                url = "https://amazon.com/playstation",
                createdAt = 1,
            ),
            TopSite.Frecent(
                id = 2,
                title = "Hotels",
                url = "https://www.hotels.com/",
                createdAt = 2,
            ),
            TopSite.Frecent(
                id = 3,
                title = "eBay",
                url = "https://www.ebay.com/n/all-categories",
                createdAt = 3,
            ),
        )

        assertTrue(topSites.hasHost("https://amazon.com"))
        assertTrue(topSites.hasHost("https://hotels.com"))
        assertTrue(topSites.hasHost("http://ebay.com"))
        assertFalse(topSites.hasHost("http://google.com"))
    }
}
