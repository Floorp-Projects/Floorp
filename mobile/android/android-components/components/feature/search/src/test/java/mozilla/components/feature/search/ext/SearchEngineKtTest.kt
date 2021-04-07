/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SearchEngineKtTest {
    @Test
    fun `Create search URL for startpage`() {
        val searchEngine = SearchEngine(
            id = UUID.randomUUID().toString(),
            name = "Escosia",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf(
                "https://www.startpage.com/sp/search?q={searchTerms}"
            )
        )

        assertEquals(
            "https://www.startpage.com/sp/search?q=Hello%20World",
            searchEngine.buildSearchUrl("Hello World")
        )
    }

    @Test
    fun `Create search URL for ecosia`() {
        val searchEngine = createSearchEngine(
            name = "Ecosia",
            icon = mock(),
            url = "https://www.ecosia.org/search?q={searchTerms}"
        )

        assertEquals(
            "https://www.ecosia.org/search?q=Hello%20World",
            searchEngine.buildSearchUrl("Hello World")
        )
    }
}
