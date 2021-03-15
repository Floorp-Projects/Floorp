/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.search.telemetry.SearchProviderModel

class SearchProviderModelTest {

    private val testSearchProvider =
        SearchProviderModel(
            name = "test",
            regexp = "test",
            queryParam = "test",
            codeParam = "test",
            codePrefixes = listOf(),
            followOnParams = listOf(),
            extraAdServersRegexps = listOf(
                "^https:\\/\\/www\\.bing\\.com\\/acli?c?k",
                "^https:\\/\\/www\\.bing\\.com\\/fd\\/ls\\/GLinkPingPost\\.aspx.*acli?c?k"
            )
        )

    @Test
    fun `test search provider contains ads`() {
        val ad = "https://www.bing.com/aclick"
        val nonAd = "https://www.bing.com/notanad"
        assertTrue(testSearchProvider.containsAdLinks(listOf(ad, nonAd)))
    }

    @Test
    fun `test search provider does not contain ads`() {
        val nonAd1 = "https://www.yahoo.com/notanad"
        val nonAd2 = "https://www.google.com/"
        assertFalse(testSearchProvider.containsAdLinks(listOf(nonAd1, nonAd2)))
    }
}
