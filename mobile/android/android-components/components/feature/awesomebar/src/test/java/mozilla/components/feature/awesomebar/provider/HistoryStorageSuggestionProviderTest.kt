/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.SearchResult
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class HistoryStorageSuggestionProviderTest {

    @Before
    fun setup() {
        Facts.clearProcessors()
    }

    @Test
    fun `Provider returns empty list when text is empty`() = runTest {
        val provider = HistoryStorageSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `provider cleanups all previous read operations when text is empty`() = runTest {
        val provider = HistoryStorageSuggestionProvider(mock(), mock())

        provider.onInputChanged("")

        verify(provider.historyStorage).cancelReads()
    }

    @Test
    fun `provider cleanups all previous read operations when text is not empty`() = runTest {
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com/", 10)))
            .`when`(history).getSuggestions(anyString(), anyInt())
        val provider = HistoryStorageSuggestionProvider(history, mock())
        val orderVerifier = inOrder(history)

        provider.onInputChanged("moz")

        orderVerifier.verify(provider.historyStorage).cancelReads()
        orderVerifier.verify(provider.historyStorage).getSuggestions(eq("moz"), anyInt())
    }

    @Test
    fun `Provider returns suggestions from configured history storage`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(listOf(SearchResult("id", "http://www.mozilla.com/", 10))).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())
        val provider = HistoryStorageSuggestionProvider(history, mock())

        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals("http://www.mozilla.com/", suggestions[0].description)
    }

    @Test
    fun `WHEN provider is set to not show edit suggestions THEN edit suggestion is set to null`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(listOf(SearchResult("id", "http://www.mozilla.com/", 10))).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())
        val provider = HistoryStorageSuggestionProvider(history, mock(), showEditSuggestion = false)

        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals("http://www.mozilla.com/", suggestions[0].description)
        assertNull(suggestions[0].editSuggestion)
    }

    @Test
    fun `Provider limits number of returned suggestions to a max of 20 by default`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(
            (1..100).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        val provider = HistoryStorageSuggestionProvider(history, mock())
        val suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)
    }

    @Test
    fun `Provider allows lowering the number of returned suggestions beneath the default`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        val provider = HistoryStorageSuggestionProvider(
            historyStorage = history,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 2,
        )

        val suggestions = provider.onInputChanged("moz")
        assertEquals(2, suggestions.size)
    }

    @Test
    fun `Provider allows increasing the number of returned suggestions above the default`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        val provider = HistoryStorageSuggestionProvider(
            historyStorage = history,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 22,
        )

        val suggestions = provider.onInputChanged("moz")
        assertEquals(22, suggestions.size)
    }

    @Test
    fun `WHEN provider max number of suggestions is changed THEN the number of return suggestions is updated`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        val provider = HistoryStorageSuggestionProvider(
            historyStorage = history,
            loadUrlUseCase = mock(),
        )

        var suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)

        provider.setMaxNumberOfSuggestions(2)
        suggestions = provider.onInputChanged("moz")
        assertEquals(2, suggestions.size)

        provider.setMaxNumberOfSuggestions(22)
        suggestions = provider.onInputChanged("moz")
        assertEquals(22, suggestions.size)

        provider.setMaxNumberOfSuggestions(45)
        suggestions = provider.onInputChanged("moz")
        assertEquals(45, suggestions.size)

        provider.setMaxNumberOfSuggestions(0)
        suggestions = provider.onInputChanged("moz")
        assertEquals(45, suggestions.size)
    }

    @Test
    fun `WHEN reset provider max number of suggestions THEN the number of return suggestions is reset to default`() = runTest {
        val history: HistoryStorage = mock()
        Mockito.doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        val provider = HistoryStorageSuggestionProvider(
            historyStorage = history,
            loadUrlUseCase = mock(),
        )

        var suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)

        provider.setMaxNumberOfSuggestions(45)
        suggestions = provider.onInputChanged("moz")
        assertEquals(45, suggestions.size)

        provider.resetToDefaultMaxSuggestions()
        suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)
    }

    @Test
    fun `Provider dedupes suggestions`() = runTest {
        val storage: HistoryStorage = mock()

        val provider = HistoryStorageSuggestionProvider(storage, mock())

        val mozSuggestions = listOf(
            SearchResult(id = "http://www.mozilla.com/", url = "http://www.mozilla.com/", score = 1),
            SearchResult(id = "http://www.mozilla.com/", url = "http://www.mozilla.com/", score = 2),
            SearchResult(id = "http://www.mozilla.com/", url = "http://www.mozilla.com/", score = 3),
        )

        val pocketSuggestions = listOf(
            SearchResult(id = "http://www.getpocket.com/", url = "http://www.getpocket.com/", score = 5),
        )

        val exampleSuggestions = listOf(
            SearchResult(id = "http://www.example.com", url = "http://www.example.com/", score = 2),
        )

        `when`(storage.getSuggestions(eq("moz"), eq(DEFAULT_HISTORY_SUGGESTION_LIMIT))).thenReturn(mozSuggestions)
        `when`(storage.getSuggestions(eq("pocket"), eq(DEFAULT_HISTORY_SUGGESTION_LIMIT))).thenReturn(pocketSuggestions)
        `when`(storage.getSuggestions(eq("www"), eq(DEFAULT_HISTORY_SUGGESTION_LIMIT))).thenReturn(pocketSuggestions + mozSuggestions + exampleSuggestions)

        var results = provider.onInputChanged("moz")
        assertEquals(1, results.size)
        assertEquals(3, results[0].score)

        results = provider.onInputChanged("pocket")
        assertEquals(1, results.size)
        assertEquals(5, results[0].score)

        results = provider.onInputChanged("www")
        assertEquals(3, results.size)
        assertEquals(5, results[0].score)
        assertEquals(3, results[1].score)
        assertEquals(2, results[2].score)
    }

    @Test
    fun `provider calls speculative connect for URL of highest scored suggestion`() = runTest {
        val history: HistoryStorage = mock()
        val engine: Engine = mock()
        val provider = HistoryStorageSuggestionProvider(history, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        Mockito.doReturn(listOf(SearchResult("id", "http://www.mozilla.com/", 10))).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals("http://www.mozilla.com/", suggestions[0].description)
        verify(engine, times(1)).speculativeConnect(suggestions[0].description!!)
    }

    @Test
    fun `fact is emitted when suggestion is clicked`() = runTest {
        val history: HistoryStorage = mock()
        val engine: Engine = mock()
        val provider = HistoryStorageSuggestionProvider(history, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        Mockito.doReturn(listOf(SearchResult("id", "http://www.mozilla.com/", 10))).`when`(history).getSuggestions(eq("moz"), Mockito.anyInt())

        suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)

        val emittedFacts = mutableListOf<Fact>()
        Facts.registerProcessor(
            object : FactProcessor {
                override fun process(fact: Fact) {
                    emittedFacts.add(fact)
                }
            },
        )

        suggestions[0].onSuggestionClicked?.invoke()
        assertTrue(emittedFacts.isNotEmpty())
        assertEquals(
            Fact(
                Component.FEATURE_AWESOMEBAR,
                Action.INTERACTION,
                AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED,
            ),
            emittedFacts.first(),
        )
    }
}
