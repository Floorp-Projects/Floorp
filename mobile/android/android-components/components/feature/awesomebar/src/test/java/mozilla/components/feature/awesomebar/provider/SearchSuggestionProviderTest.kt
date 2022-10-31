/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.core.graphics.drawable.toBitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.awesomebar.R
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.IOException

private const val GOOGLE_MOCK_RESPONSE = "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"
private const val GOOGLE_MOCK_RESPONSE_WITH_DUPLICATES = "[\"firefox\",[\"firefox\",\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class SearchSuggestionProviderTest {
    @Test
    fun `Provider returns suggestion with chips based on search engine suggestion`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

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

                verify(useCase, never()).invoke(anyString(), any(), any())

                CollectionProcessor.withFactCollection { facts ->
                    suggestion.onChipClicked!!.invoke(suggestion.chips[6])

                    assertEquals(1, facts.size)
                    facts[0].apply {
                        assertEquals(Component.FEATURE_AWESOMEBAR, component)
                        assertEquals(Action.INTERACTION, action)
                        assertEquals(AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED, item)
                    }
                }

                verify(useCase).invoke(eq("firefox focus"), any(), any())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns multiple suggestions in MULTIPLE mode`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
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

                verify(useCase, never()).invoke(anyString(), any(), any())

                CollectionProcessor.withFactCollection { facts ->
                    suggestions[6].onSuggestionClicked!!.invoke()

                    assertEquals(1, facts.size)
                    facts[0].apply {
                        assertEquals(Component.FEATURE_AWESOMEBAR, component)
                        assertEquals(Action.INTERACTION, action)
                        assertEquals(AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED, item)
                    }
                }

                verify(useCase).invoke(eq("firefox focus"), any(), any())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns multiple suggestions with limit`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                limit = 5,
            )

            try {
                val suggestions = provider.onInputChanged("fire")

                println(suggestions)

                assertEquals(5, suggestions.size)

                assertEquals("fire", suggestions[0].title)
                assertEquals("firefox", suggestions[1].title)
                assertEquals("firefox for mac", suggestions[2].title)
                assertEquals("firefox quantum", suggestions[3].title)
                assertEquals("firefox update", suggestions[4].title)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns chips with limit`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider =
                SearchSuggestionProvider(searchEngine, useCase, HttpURLConnectionClient(), limit = 5)

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)

                val suggestion = suggestions[0]
                assertEquals(5, suggestion.chips.size)

                assertEquals("fire", suggestion.chips[0].title)
                assertEquals("firefox", suggestion.chips[1].title)
                assertEquals("firefox for mac", suggestion.chips[2].title)
                assertEquals("firefox quantum", suggestion.chips[3].title)
                assertEquals("firefox update", suggestion.chips[4].title)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider should use engine icon by default`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val engineIcon = testContext.getDrawable(R.drawable.mozac_ic_device_desktop)!!.toBitmap()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = engineIcon,
                suggestUrl = server.url("/").toString(),
            )

            val provider = SearchSuggestionProvider(searchEngine, mock(), HttpURLConnectionClient())
            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)
                assertTrue(suggestions[0].icon?.sameAs(engineIcon)!!)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider should use icon parameter when available`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val engineIcon = testContext.getDrawable(R.drawable.mozac_ic_device_desktop)!!.toBitmap()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = engineIcon,
                suggestUrl = server.url("/").toString(),
            )

            val paramIcon = testContext.getDrawable(R.drawable.mozac_ic_search)!!.toBitmap()

            val provider = SearchSuggestionProvider(
                searchEngine,
                mock(),
                HttpURLConnectionClient(),
                icon = paramIcon,
            )

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)
                assertTrue(suggestions[0].icon?.sameAs(paramIcon)!!)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns empty list if text is empty`() = runTest {
        val provider = SearchSuggestionProvider(mock(), mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider should return default suggestion for search engine that cannot provide suggestion`() =
        runTest {
            val searchEngine = createSearchEngine(
                name = "Test",
                url = "https://localhost/?q={searchTerms}",
                icon = mock(),
            )
            val provider = SearchSuggestionProvider(searchEngine, mock(), mock())

            val suggestions = provider.onInputChanged("fire")
            assertEquals(1, suggestions.size)

            val suggestion = suggestions[0]
            assertEquals(1, suggestion.chips.size)

            assertEquals("fire", suggestion.chips[0].title)
        }

    @Test
    fun `Provider doesn't fail if fetch returns HTTP error`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setResponseCode(404).setBody("error"))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

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
        runTest {
            val searchEngine = createSearchEngine(
                name = "Test",
                url = "https://localhost/?q={searchTerms}",
                icon = mock(),
                suggestUrl = "https://localhost/suggestions",
            )
            val useCase: SearchUseCases.SearchUseCase = mock()

            val client = object : Client() {
                override fun fetch(request: Request): Response {
                    throw IOException()
                }
            }

            val provider =
                SearchSuggestionProvider(searchEngine, useCase, client)
            val suggestions = provider.onInputChanged("fire")
            assertEquals(1, suggestions.size)

            val suggestion = suggestions[0]
            assertEquals(1, suggestion.chips.size)
            assertEquals("fire", suggestion.chips[0].title)
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `Constructor throws if limit is less than 1`() {
        SearchSuggestionProvider(mock(), mock(), mock(), limit = 0)
    }

    @Test
    fun `Provider returns distinct multiple suggestions`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE_WITH_DUPLICATES))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
            )

            try {
                val suggestions = provider.onInputChanged("fire")
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
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider returns multiple suggestions with limit and no description`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                limit = 3,
                showDescription = false,
            )

            try {
                val suggestions = provider.onInputChanged("fire")

                println(suggestions)

                assertEquals(3, suggestions.size)

                assertEquals("fire", suggestions[0].title)
                assertEquals("firefox", suggestions[1].title)
                assertEquals("firefox for mac", suggestions[2].title)
                assertNull(suggestions[0].description)
                assertNull(suggestions[1].description)
                assertNull(suggestions[2].description)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider calls speculativeConnect for URL of highest scored suggestion in MULTIPLE mode`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val engine: Engine = mock()
            val provider = SearchSuggestionProvider(
                searchEngine,
                mock(),
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                engine = engine,
                limit = 3,
                showDescription = false,
            )

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(3, suggestions.size)
                assertEquals("fire", suggestions[0].title)
                verify(engine, times(1))
                    .speculativeConnect(server.url("/search?q=fire").toString())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider calls speculativeConnect for URL of highest scored chip in SINGLE mode`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val engine: Engine = mock()
            val provider = SearchSuggestionProvider(
                searchEngine,
                mock(),
                HttpURLConnectionClient(),
                engine = engine,
            )

            try {
                val suggestions = provider.onInputChanged("fire")
                assertEquals(1, suggestions.size)

                val suggestion = suggestions[0]
                assertEquals(11, suggestion.chips.size)
                assertEquals("fire", suggestion.chips[0].title)
                verify(engine, times(1))
                    .speculativeConnect(server.url("/search?q=fire").toString())
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider filters exact match from multiple suggestions`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE_WITH_DUPLICATES))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                filterExactMatch = true,
            )

            try {
                val suggestions = provider.onInputChanged("firefox")
                assertEquals(10, suggestions.size)
                assertEquals("firefox for mac", suggestions[1].title)
                assertEquals("firefox quantum", suggestions[2].title)
                assertEquals("firefox update", suggestions[3].title)
                assertEquals("firefox esr", suggestions[4].title)
                assertEquals("firefox focus", suggestions[5].title)
                assertEquals("firefox addons", suggestions[6].title)
                assertEquals("firefox extensions", suggestions[7].title)
                assertEquals("firefox nightly", suggestions[8].title)
                assertEquals("firefox clear cache", suggestions[9].title)
            } finally {
                server.shutdown()
            }
        }
    }

    @Test
    fun `Provider filters chips with exact match`() {
        runTest {
            val server = MockWebServer()
            server.enqueue(MockResponse().setBody(GOOGLE_MOCK_RESPONSE))
            server.start()

            val searchEngine = createSearchEngine(
                name = "Test",
                url = server.url("/search?q={searchTerms}").toString(),
                icon = mock(),
                suggestUrl = server.url("/").toString(),
            )

            val useCase: SearchUseCases.SearchUseCase = mock()

            val provider = SearchSuggestionProvider(
                searchEngine,
                useCase,
                HttpURLConnectionClient(),
                filterExactMatch = true,
            )

            try {
                val suggestions = provider.onInputChanged("firefox")
                assertEquals(1, suggestions.size)

                val suggestion = suggestions[0]
                assertEquals(9, suggestion.chips.size)

                assertEquals("firefox for mac", suggestion.chips[0].title)
                assertEquals("firefox quantum", suggestion.chips[1].title)
                assertEquals("firefox update", suggestion.chips[2].title)
                assertEquals("firefox esr", suggestion.chips[3].title)
                assertEquals("firefox focus", suggestion.chips[4].title)
                assertEquals("firefox addons", suggestion.chips[5].title)
                assertEquals("firefox extensions", suggestion.chips[6].title)
                assertEquals("firefox nightly", suggestion.chips[7].title)
                assertEquals("firefox clear cache", suggestion.chips[8].title)
            } finally {
                server.shutdown()
            }
        }
    }
}
