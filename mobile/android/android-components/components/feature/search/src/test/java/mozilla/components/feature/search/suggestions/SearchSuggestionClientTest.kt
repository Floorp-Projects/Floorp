/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.suggestions

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class SearchSuggestionClientTest {
    companion object {
        val GOOGLE_MOCK_RESPONSE: SearchSuggestionFetcher = { "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]" }
        val QWANT_MOCK_RESPONSE: SearchSuggestionFetcher = { "{\"status\":\"success\",\"data\":{\"items\":[{\"value\":\"firefox (video game)\",\"suggestType\":3},{\"value\":\"firefox addons\",\"suggestType\":12},{\"value\":\"firefox\",\"suggestType\":2},{\"value\":\"firefox quantum\",\"suggestType\":12},{\"value\":\"firefox focus\",\"suggestType\":12}],\"special\":[],\"availableQwick\":[]}}" }
        val SERVER_ERROR_RESPONSE: SearchSuggestionFetcher = { "Server error. Try again later" }
    }

    private val searchEngine = createSearchEngine(
        name = "Test",
        url = "https://localhost?q={searchTerms}",
        suggestUrl = "https://localhost/suggestions?q={searchTerms}",
        icon = mock(),
    )

    @Test
    fun `Get a list of results based on the Google search engine`() = runTest {
        val client = SearchSuggestionClient(searchEngine, GOOGLE_MOCK_RESPONSE)
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")

        val results = client.getSuggestions("firefox")

        assertEquals(expectedResults, results)
    }

    @Test
    fun `Get a list of results based on a non google search engine`() = runTest {
        val qwant = createSearchEngine(
            name = "Qwant",
            url = "https://localhost?q={searchTerms}",
            suggestUrl = "https://localhost/suggestions?q={searchTerms}",
            icon = mock(),
        )
        val client = SearchSuggestionClient(qwant, QWANT_MOCK_RESPONSE)
        val expectedResults = listOf("firefox (video game)", "firefox addons", "firefox", "firefox quantum", "firefox focus")

        val results = client.getSuggestions("firefox")

        assertEquals(expectedResults, results)
    }

    @Test(expected = SearchSuggestionClient.ResponseParserException::class)
    fun `Check that a bad response will throw a parser exception`() = runTest {
        val client = SearchSuggestionClient(searchEngine, SERVER_ERROR_RESPONSE)

        client.getSuggestions("firefox")
    }

    @Test(expected = SearchSuggestionClient.FetchException::class)
    fun `Check that an exception in the suggestionFetcher will re-throw an IOException`() = runTest {
        val client = SearchSuggestionClient(searchEngine) { throw IOException() }

        client.getSuggestions("firefox")
    }

    @Test
    fun `Check that a search engine without a suggestURI will return an empty suggestion list`() = runTest {
        val searchEngine = createSearchEngine(
            name = "Test",
            url = "https://localhost?q={searchTerms}",
            icon = mock(),
        )
        val client = SearchSuggestionClient(searchEngine) { "no-op" }

        val results = client.getSuggestions("firefox")

        assertEquals(emptyList<String>(), results)
    }

    @Test
    fun `Default search engine is used if search engine manager provided`() = runTest {
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine),
                ),
            ),
        )

        val client = SearchSuggestionClient(
            testContext,
            store,
            GOOGLE_MOCK_RESPONSE,
        )
        val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")

        val results = client.getSuggestions("firefox")

        assertEquals(expectedResults, results)
    }
}
