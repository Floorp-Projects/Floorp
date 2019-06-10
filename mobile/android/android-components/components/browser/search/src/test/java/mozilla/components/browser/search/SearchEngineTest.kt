/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.graphics.Bitmap
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SearchEngineTest {

    @Test
    fun `Build and verify basic search URL for search engine`() {
        val resultUri = Uri.parse("https://www.mozilla.org/?q={searchTerms}")

        val searchEngine = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(resultUri))

        assertEquals(
                "https://www.mozilla.org/?q=hello%20world",
                searchEngine.buildSearchUrl("hello world"))
    }

    @Test
    fun `Build search URL with all possible parameters`() {
        val resultUri = Uri.parse(
                "https://mozilla.org/?" +
                    "locale={moz:locale}" +
                    "&dist={moz:distributionID}" +
                    "&official={moz:official}" +
                    "&q={searchTerms}" +
                    "&input={inputEncoding}" +
                    "&output={outputEncoding}" +
                    "&lang={language}" +
                    "&random={random}" +
                    "&foo={foo:bar}")

        val searchEngine = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(resultUri))

        assertEquals(
                "https://mozilla.org/?locale=en_US&dist=&official=unofficial&q=hello%20world&input=UTF-8&output=UTF-8&lang=en_US&random=&foo=",
                searchEngine.buildSearchUrl("hello world"))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `constructor throws if list of uris is empty`() {
        SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                emptyList())
    }

    @Test
    fun `result url will be normalized`() {
        val resultUri = Uri.parse("   mozilla.org/?q={searchTerms}   ")

        val searchEngine = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(resultUri))

        assertEquals(
                "http://mozilla.org/?q=hello%20world",
                searchEngine.buildSearchUrl("hello world"))
    }

    @Test
    fun `Build search suggestion URL`() {
        val searchUri = Uri.parse("https://mozilla.org/search/?q={searchTerms}")
        val suggestionsUri = Uri.parse("https://mozilla.org/search/suggestions?q={searchTerms}")

        val searchEngine = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(searchUri),
                suggestionsUri)

        assertEquals(
                "https://mozilla.org/search/suggestions?q=Focus",
                searchEngine.buildSuggestionsURL("Focus"))
    }

    private fun mockResultUriList(): List<Uri> = listOf(
            Uri.parse("http://${UUID.randomUUID()}).mozilla.org/q?={searchTerms}")
    )

    @Test
    fun `can show that it can provide search suggestions`() {
        val searchUri = Uri.parse("https://mozilla.org/search/?q={searchTerms}")
        val suggestionsUri = Uri.parse("https://mozilla.org/search/suggestions?q={searchTerms}")

        val searchEngineWithSuggestions = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(searchUri),
                suggestionsUri)

        val searchEngineWithoutSuggestions = SearchEngine(
                "mozsearch",
                "Mozilla Search",
                mock(Bitmap::class.java),
                listOf(searchUri))

        assertTrue(searchEngineWithSuggestions.canProvideSearchSuggestions)
        assertFalse(searchEngineWithoutSuggestions.canProvideSearchSuggestions)
    }
}
