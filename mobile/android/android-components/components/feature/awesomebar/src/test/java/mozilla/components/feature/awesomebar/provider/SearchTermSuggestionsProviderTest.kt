/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.graphics.Bitmap
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.OS_SEARCH_ENGINE_TERMS_PARAM
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.DocumentType.Media
import mozilla.components.concept.storage.DocumentType.Regular
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.search.SearchUseCases.SearchUseCase
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class SearchTermSuggestionsProviderTest {
    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com/search?q=firefox", "fire", null),
        title = "Firefox",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = Regular,
        previewImageUrl = null,
    )
    private var searchEngine: SearchEngine = mock()
    private val storage: PlacesHistoryStorage = mock()

    @Before
    fun setup() = runTest {
        doReturn("http://www.mozilla.com/search?q=firefox".toUri()).`when`(searchEngine).resultsUrl
        doReturn(listOf(historyEntry)).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a too large number of suggestions WHEN constructing the provider THEN throw an exception`() {
        SearchTermSuggestionsProvider(
            historyStorage = mock(),
            searchUseCase = mock(),
            searchEngine = searchEngine,
            maxNumberOfSuggestions = SEARCH_TERMS_MAXIMUM_ALLOWED_SUGGESTIONS_LIMIT + 1,
        )
    }

    @Test
    fun `GIVEN an empty input WHEN querying suggestions THEN return an empty list`() = runTest {
        val provider = SearchTermSuggestionsProvider(mock(), mock(), searchEngine)

        val suggestions = provider.onInputChanged("")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `GIVEN an empty input WHEN querying suggestions THEN cleanup all read operations for the current query`() = runTest {
        val provider = SearchTermSuggestionsProvider(storage, mock(), searchEngine)

        provider.onInputChanged("")

        verify(storage, never()).cancelReads()
        verify(storage).cancelReads("")
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN cleanup all read operations for the current query`() = runTest {
        val provider = SearchTermSuggestionsProvider(storage, mock(), searchEngine)
        val orderVerifier = inOrder(storage)

        provider.onInputChanged("fir")

        orderVerifier.verify(storage, never()).cancelReads()
        orderVerifier.verify(storage).cancelReads("fir")
        orderVerifier.verify(storage).getHistoryMetadataSince(Long.MIN_VALUE)
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN return suggestions from configured history storage`() = runTest {
        val searchEngineIcon: Bitmap = mock()
        val provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            icon = searchEngineIcon,
            showEditSuggestion = true,
        )

        val suggestions = provider.onInputChanged("fir")

        assertEquals(1, suggestions.size)
        assertEquals(provider, suggestions[0].provider)
        assertEquals(historyEntry.key.searchTerm, suggestions[0].title)
        assertNull(suggestions[0].description)
        assertEquals(historyEntry.key.searchTerm, suggestions[0].editSuggestion)
        assertEquals(searchEngineIcon, suggestions[0].icon)
        assertNull(suggestions[0].indicatorIcon)
        assertTrue(suggestions[0].chips.isEmpty())
        assertTrue(suggestions[0].flags.isEmpty())
        assertNotNull(suggestions[0].onSuggestionClicked)
        assertNull(suggestions[0].onChipClicked)
        assertEquals(Int.MAX_VALUE - 2, suggestions[0].score)
    }

    @Test
    fun `GIVEN a valid input and no provided icon WHEN querying suggestions THEN return suggestions showing the search engine icon`() = runTest {
        val searchEngineIcon: Bitmap = mock()

        // Test with a provided icon.
        var provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            icon = searchEngineIcon,
        )
        var suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals(searchEngineIcon, suggestions[0].icon)

        // Test with no provided icon.
        provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            icon = null,
        )
        suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals(searchEngine.icon, suggestions[0].icon)
    }

    @Test
    fun `GIVEN a valid input and editing suggestions disabled WHEN querying suggestions THEN return suggestions with no string for editing the suggestion`() = runTest {
        var provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            showEditSuggestion = true,
        )
        var suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals(historyEntry.key.searchTerm, suggestions[0].editSuggestion)

        provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            showEditSuggestion = false,
        )
        suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals(null, suggestions[0].editSuggestion)
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN return suggestions configured to do a new search when clicked`() = runTest {
        val searchUseCase: SearchUseCase = mock()
        doReturn(listOf(historyEntry)).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
        val provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = searchUseCase,
            searchEngine = searchEngine,
        )

        val suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        suggestions[0].onSuggestionClicked?.invoke()

        verify(searchUseCase).invoke(historyEntry.key.searchTerm!!, null, null)
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN return suggestions configured to emit a telemetry fact when clicked`() = runTest {
        val searchUseCase: SearchUseCase = mock()
        doReturn(listOf(historyEntry)).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
        val provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = searchUseCase,
            searchEngine = searchEngine,
        )

        val suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        CollectionProcessor.withFactCollection { facts ->
            suggestions[0].onSuggestionClicked?.invoke()

            assertEquals(1, facts.size)
            with(facts[0]) {
                assertEquals(Component.FEATURE_AWESOMEBAR, component)
                assertEquals(Action.INTERACTION, action)
                assertEquals(AwesomeBarFacts.Items.SEARCH_TERM_SUGGESTION_CLICKED, item)
            }
        }
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN return by default 2 suggestions`() = runTest {
        val historyEntries = listOf(
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "fir"),
                createdAt = 1,
            ),
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "fire"),
                createdAt = 2,
            ),
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "firefox"),
                createdAt = 3,
            ),
        )
        doReturn(historyEntries).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
        val provider = SearchTermSuggestionsProvider(storage, mock(), searchEngine)

        val suggestions = provider.onInputChanged("fir")

        assertEquals(2, suggestions.size)
        assertEquals(historyEntries[2].key.searchTerm, suggestions[0].title)
        assertEquals(Int.MAX_VALUE - 2, suggestions[0].score)
        assertEquals(historyEntries[1].key.searchTerm, suggestions[1].title)
        assertEquals(Int.MAX_VALUE - 3, suggestions[1].score)
    }

    @Test
    fun `GIVEN a valid input and a different number of suggestions required WHEN querying suggestions THEN return the require number of suggestions if available`() = runTest {
        val historyEntries = listOf(
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "fir"),
                createdAt = 1,
            ),
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "fire"),
                createdAt = 2,
            ),
            historyEntry.copy(
                key = historyEntry.key.copy(searchTerm = "firef"),
                createdAt = 3,
            ),
        )
        doReturn(historyEntries).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)

        // Test with asking for more suggestions.
        var provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            maxNumberOfSuggestions = 3,
        )
        var suggestions = provider.onInputChanged("fir")
        assertEquals(3, suggestions.size)
        assertEquals(historyEntries[2].key.searchTerm, suggestions[0].title)
        assertEquals(historyEntries[1].key.searchTerm, suggestions[1].title)
        assertEquals(historyEntries[0].key.searchTerm, suggestions[2].title)

        // Test with asking for fewer suggestions.
        provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            maxNumberOfSuggestions = 1,
        )
        suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals(historyEntries[2].key.searchTerm, suggestions[0].title)
    }

    @Test
    fun `GIVEN a valid input WHEN querying suggestions THEN return suggestions with different search terms`() = runTest {
        val historyEntries = listOf(
            historyEntry,
            historyEntry.copy(
                createdAt = 1,
                documentType = Media,
            ),
        )
        doReturn(historyEntries).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
        val provider = SearchTermSuggestionsProvider(storage, mock(), searchEngine)

        val suggestions = provider.onInputChanged("fir")

        assertEquals(1, suggestions.size)
        assertEquals(historyEntries[0].key.searchTerm, suggestions[0].title)
    }

    @Test
    fun `GIVEN a new user input WHEN building search term suggestions THEN do a speculative connect to the url for the first search term`() = runTest {
        val engine: Engine = mock()
        val searcheEngineSearchUrl = "https://test/q=$OS_SEARCH_ENGINE_TERMS_PARAM"
        doReturn(listOf(searcheEngineSearchUrl)).`when`(searchEngine).resultUrls
        val provider = SearchTermSuggestionsProvider(
            historyStorage = storage,
            searchUseCase = mock(),
            searchEngine = searchEngine,
            engine = engine,
        )

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        doReturn(listOf(historyEntry)).`when`(storage).getHistoryMetadataSince(Long.MIN_VALUE)
        suggestions = provider.onInputChanged("fir")
        assertEquals(1, suggestions.size)
        assertEquals("fire", suggestions[0].title)
        assertNull(suggestions[0].description)
        verify(engine, times(1)).speculativeConnect(
            searcheEngineSearchUrl.replace(OS_SEARCH_ENGINE_TERMS_PARAM, suggestions[0].title!!),
        )
    }
}
