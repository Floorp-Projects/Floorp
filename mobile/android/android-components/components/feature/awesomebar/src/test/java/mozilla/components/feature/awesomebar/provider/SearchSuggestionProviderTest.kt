/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import junit.framework.Assert.assertEquals
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

private const val GOOGLE_MOCK_RESPONSE = "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"

@RunWith(RobolectricTestRunner::class)
class SearchSuggestionProviderTest {
    @Test
    fun `Provider returns suggestion with chips based on search engine suggestion`() {
        runBlocking {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine: SearchEngine = mock()
            doReturn(server.url("/").toString())
                .`when`(searchEngine).buildSuggestionsURL("fire")
            doReturn(true).`when`(searchEngine).canProvideSearchSuggestions
            doReturn("google").`when`(searchEngine).name

            val searchEngineManager: SearchEngineManager = mock()
            doReturn(searchEngine).`when`(searchEngineManager).getDefaultSearchEngine(any(), any())

            val useCase = spy(SearchUseCases(
                RuntimeEnvironment.application,
                searchEngineManager,
                SessionManager(mock()).apply { add(Session("https://www.mozilla.org")) }
            ).defaultSearch)
            doNothing().`when`(useCase).invoke(anyString(), any<Session>(), any<SearchEngine>())

            val provider =
                SearchSuggestionProvider(searchEngine, useCase, HttpURLConnectionClient())

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)

                val suggestion = suggestions[0]
                assertEquals(11, suggestion.chips.size)

                assertEquals("fire", suggestion.chips[0].title)
                assertEquals("firefox", suggestion.chips[1].title)
                assertEquals("firefox for mac", suggestion.chips[2].title)
                assertEquals("firefox quantum", suggestion.chips[3].title)
                assertEquals("firefox update", suggestion.chips[4].title)
                assertEquals("firefox esr", suggestion.chips[5].title)
                assertEquals("firefox focus", suggestion.chips[6].title)
                assertEquals("firefox addons", suggestion.chips[7].title)
                assertEquals("firefox extensions", suggestion.chips[8].title)
                assertEquals("firefox nightly", suggestion.chips[9].title)
                assertEquals("firefox clear cache", suggestion.chips[10].title)

                verify(useCase, never()).invoke(anyString(), any<Session>(), any<SearchEngine>())

                suggestion.onChipClicked!!.invoke(suggestion.chips[6])

                verify(useCase).invoke(eq("firefox focus"), any<Session>(), any<SearchEngine>())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns multiple suggestions in MULTIPLE mode`() {
        runBlocking {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine: SearchEngine = mock()
            doReturn(server.url("/").toString())
                .`when`(searchEngine).buildSuggestionsURL("fire")
            doReturn(true).`when`(searchEngine).canProvideSearchSuggestions
            doReturn("google").`when`(searchEngine).name

            val searchEngineManager: SearchEngineManager = mock()
            doReturn(searchEngine).`when`(searchEngineManager).getDefaultSearchEngine(any(), any())

            val useCase = spy(SearchUseCases(
                RuntimeEnvironment.application,
                searchEngineManager,
                SessionManager(mock()).apply { add(Session("https://www.mozilla.org")) }
            ).defaultSearch)
            doNothing().`when`(useCase).invoke(anyString(), any<Session>(), any<SearchEngine>())

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS
            )

            try {
                val suggestions = provider.onInputChanged("fire")

                println(suggestions)

                assertEquals(11, suggestions.size)

                assertEquals("fire", suggestions[0].title)
                assertEquals("firefox", suggestions[1].title)
                assertEquals("firefox for mac", suggestions[2].title)
                assertEquals("firefox quantum", suggestions[3].title)
                assertEquals("firefox update", suggestions[4].title)
                assertEquals("firefox esr", suggestions[5].title)
                assertEquals("firefox focus", suggestions[6].title)
                assertEquals("firefox addons", suggestions[7].title)
                assertEquals("firefox extensions", suggestions[8].title)
                assertEquals("firefox nightly", suggestions[9].title)
                assertEquals("firefox clear cache", suggestions[10].title)

                verify(useCase, never()).invoke(anyString(), any<Session>(), any<SearchEngine>())

                suggestions[6].onSuggestionClicked!!.invoke()

                verify(useCase).invoke(eq("firefox focus"), any<Session>(), any<SearchEngine>())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider should not clear suggestions`() {
        val provider = SearchSuggestionProvider(mock(), mock(), mock())
        assertFalse(provider.shouldClearSuggestions)
    }

    @Test
    fun `Provider returns empty list if text is empty`() = runBlocking {
        val provider = SearchSuggestionProvider(mock(), mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider should return default suggestion for search engine that cannot provide suggestion`() =
        runBlocking {
            val searchEngine: SearchEngine = mock()
            doReturn(false).`when`(searchEngine).canProvideSearchSuggestions

            val provider = SearchSuggestionProvider(searchEngine, mock(), mock())

            val suggestions = provider.onInputChanged("fire")
            assertEquals(1, suggestions.size)

            val suggestion = suggestions[0]
            assertEquals(1, suggestion.chips.size)

            assertEquals("fire", suggestion.chips[0].title)
        }

    @Test
    fun `Provider doesn't fail if fetch returns HTTP error`() {
        runBlocking {
            val server = MockWebServer()
            server.enqueue(MockResponse().setResponseCode(404).setBody("error"))
            server.start()

            val searchEngine: SearchEngine = mock()
            doReturn(server.url("/").toString())
                .`when`(searchEngine).buildSuggestionsURL("fire")
            doReturn(true).`when`(searchEngine).canProvideSearchSuggestions
            doReturn("google").`when`(searchEngine).name

            val searchEngineManager: SearchEngineManager = mock()
            doReturn(searchEngine).`when`(searchEngineManager).getDefaultSearchEngine(any(), any())

            val useCase = spy(SearchUseCases(
                RuntimeEnvironment.application,
                searchEngineManager,
                SessionManager(mock()).apply { add(Session("https://www.mozilla.org")) }
            ).defaultSearch)
            doNothing().`when`(useCase).invoke(anyString(), any<Session>(), any<SearchEngine>())

            val provider =
                SearchSuggestionProvider(searchEngine, useCase, HttpURLConnectionClient())

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)

                val suggestion = suggestions[0]
                assertEquals(1, suggestion.chips.size)
                assertEquals("fire", suggestion.chips[0].title)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider doesn't fail if fetch throws exception`() {
        runBlocking {
            val searchEngine: SearchEngine = mock()
            doReturn("/").`when`(searchEngine).buildSuggestionsURL("fire")
            doReturn(true).`when`(searchEngine).canProvideSearchSuggestions
            doReturn("google").`when`(searchEngine).name

            val searchEngineManager: SearchEngineManager = mock()
            doReturn(searchEngine).`when`(searchEngineManager).getDefaultSearchEngine(any(), any())

            val useCase = spy(SearchUseCases(
                RuntimeEnvironment.application,
                searchEngineManager,
                SessionManager(mock()).apply { add(Session("https://www.mozilla.org")) }
            ).defaultSearch)
            doNothing().`when`(useCase).invoke(anyString(), any<Session>(), any<SearchEngine>())

            val provider =
                SearchSuggestionProvider(searchEngine, useCase, HttpURLConnectionClient())
            val suggestions = provider.onInputChanged("fire")
            assertEquals(1, suggestions.size)

            val suggestion = suggestions[0]
            assertEquals(1, suggestion.chips.size)
            assertEquals("fire", suggestion.chips[0].title)
        }
    }
}
