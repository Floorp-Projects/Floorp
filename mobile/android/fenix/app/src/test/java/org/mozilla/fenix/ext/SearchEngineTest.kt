/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.mockk
import mozilla.components.browser.state.search.SearchEngine
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

class SearchEngineTest {

    @Test
    fun `custom search engines are identified correctly`() {
        val searchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Not custom",
            icon = mockk(),
            type = SearchEngine.Type.BUNDLED,
            resultUrls = listOf(
                "https://www.startpage.com/sp/search?q={searchTerms}",
            ),
        )

        val customSearchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Custom",
            icon = mockk(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf(
                "https://www.startpage.com/sp/search?q={searchTerms}",
            ),
        )

        assertFalse(searchEngine.isCustomEngine())
        assertTrue(customSearchEngine.isCustomEngine())
    }

    @Test
    fun `well known search engines are identified correctly`() {
        val searchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Not well known",
            icon = mockk(),
            type = SearchEngine.Type.BUNDLED,
            resultUrls = listOf(
                "https://www.random.com/sp/search?q={searchTerms}",
            ),
        )

        val wellKnownSearchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Well known",
            icon = mockk(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf(
                "https://www.startpage.com/sp/search?q={searchTerms}",
            ),
        )

        assertFalse(searchEngine.isKnownSearchDomain())
        assertTrue(wellKnownSearchEngine.isKnownSearchDomain())
    }
}
