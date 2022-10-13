/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.SearchResult
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class CombinedHistorySuggestionProviderTest {

    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular,
        previewImageUrl = null,
    )

    @Test
    fun `GIVEN history items exists WHEN onInputChanged is called with empty text THEN return empty suggestions list`() = runTest {
        val metadata: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(metadata).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, metadata, mock())

        assertTrue(provider.onInputChanged("").isEmpty())
        assertTrue(provider.onInputChanged("  ").isEmpty())
    }

    @Test
    fun `WHEN onInputChanged is called with empty text THEN cancel all previous read operations`() = runTest {
        val history: HistoryStorage = mock()
        val metadata: HistoryMetadataStorage = mock()
        val provider = CombinedHistorySuggestionProvider(history, metadata, mock())

        provider.onInputChanged("")

        verify(history).cancelReads()
        verify(metadata).cancelReads()
    }

    @Test
    fun `GIVEN more suggestions asked than metadata items exist WHEN user changes input THEN return a combined list of suggestions`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com/firefox/", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())

        val result = provider.onInputChanged("moz")

        assertEquals(2, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
        assertEquals("http://www.mozilla.com/firefox/", result[1].description)
    }

    @Test
    fun `WHEN onInputChanged is called with non empty text THEN cancel all previous read operations`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(emptyList<SearchResult>()).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())
        val orderVerifier = Mockito.inOrder(storage, history)

        provider.onInputChanged("moz")

        orderVerifier.verify(history).cancelReads()
        orderVerifier.verify(storage).cancelReads()
        orderVerifier.verify(storage).queryHistoryMetadata(eq("moz"), anyInt())
        orderVerifier.verify(history).getSuggestions(eq("moz"), anyInt())
    }

    @Test
    fun `GIVEN fewer suggestions asked than metadata items exist WHEN user changes input THEN return suggestions only based on metadata items`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com/firefox/", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, storage, mock(), maxNumberOfSuggestions = 1)

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
    }

    @Test
    fun `GIVEN only storage history items exist WHEN user changes input THEN return suggestions only based on storage items`() = runTest {
        val metadata: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(metadata).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com/firefox/", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, metadata, mock(), maxNumberOfSuggestions = 1)

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com/firefox/", result[0].description)
    }

    @Test
    fun `GIVEN duplicated metadata and storage entries WHEN user changes input THEN return distinct suggestions`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
    }

    @Test
    fun `GIVEN a combined list of suggestions WHEN history results exist THEN urls are deduped and scores are adjusted`() = runTest {
        val metadataEntry1 = HistoryMetadata(
            key = HistoryMetadataKey("https://www.mozilla.com", null, null),
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        val metadataEntry2 = HistoryMetadata(
            key = HistoryMetadataKey("https://www.mozilla.com/firefox", null, null),
            title = "firefox",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 20,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        val searchResult1 = SearchResult(
            id = "1",
            url = "https://www.mozilla.com",
            title = "mozilla",
            score = 1,
        )

        val searchResult2 = SearchResult(
            id = "2",
            url = "https://www.mozilla.com/pocket",
            title = "pocket",
            score = 2,
        )

        val metadataStorage: HistoryMetadataStorage = mock()
        val historyStorage: HistoryStorage = mock()
        doReturn(listOf(metadataEntry2, metadataEntry1)).`when`(metadataStorage).queryHistoryMetadata(eq("moz"), anyInt())
        doReturn(listOf(searchResult1, searchResult2)).`when`(historyStorage).getSuggestions(eq("moz"), anyInt())

        val provider = CombinedHistorySuggestionProvider(historyStorage, metadataStorage, mock())

        val result = provider.onInputChanged("moz")

        assertEquals(3, result.size)
        assertEquals("https://www.mozilla.com/firefox", result[0].description)
        assertEquals(4, result[0].score)

        assertEquals("https://www.mozilla.com", result[1].description)
        assertEquals(3, result[1].score)

        assertEquals("https://www.mozilla.com/pocket", result[2].description)
        assertEquals(2, result[2].score)
    }

    @Test
    fun `WHEN provider is set to not show edit suggestions THEN edit suggestion is set to null`() = runTest {
        val metadata: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(metadata).queryHistoryMetadata(eq("moz"), anyInt())
        val history: HistoryStorage = mock()
        doReturn(listOf(SearchResult("id", "http://www.mozilla.com/firefox/", 10))).`when`(history).getSuggestions(eq("moz"), anyInt())
        val provider = CombinedHistorySuggestionProvider(history, metadata, mock(), maxNumberOfSuggestions = 1, showEditSuggestion = false)

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com/firefox/", result[0].description)
        assertNull(result[0].editSuggestion)
    }

    @Test
    fun `WHEN provider max number of suggestions is changed THEN the number of return suggestions is updated`() = runTest {
        val history: HistoryStorage = mock()
        val metadata: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(metadata).queryHistoryMetadata(eq("moz"), anyInt())
        doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), anyInt())

        val provider = CombinedHistorySuggestionProvider(history, metadata, mock(), showEditSuggestion = false)

        provider.setMaxNumberOfSuggestions(2)
        var suggestions = provider.onInputChanged("moz")
        assertEquals(2, suggestions.size)

        provider.setMaxNumberOfSuggestions(22)
        suggestions = provider.onInputChanged("moz")
        assertEquals(22, suggestions.size)

        provider.setMaxNumberOfSuggestions(0)
        suggestions = provider.onInputChanged("moz")
        assertEquals(22, suggestions.size)

        provider.setMaxNumberOfSuggestions(45)
        suggestions = provider.onInputChanged("moz")
        assertEquals(45, suggestions.size)
    }

    @Test
    fun `WHEN reset provider max number of suggestions THEN the number of return suggestions is reset to default`() = runTest {
        val history: HistoryStorage = mock()
        val metadata: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(metadata).queryHistoryMetadata(eq("moz"), anyInt())
        doReturn(
            (1..50).map {
                SearchResult("id$it", "http://www.mozilla.com/$it/", 10)
            },
        ).`when`(history).getSuggestions(eq("moz"), anyInt())

        val provider = CombinedHistorySuggestionProvider(history, metadata, mock(), showEditSuggestion = false)

        var suggestions = provider.onInputChanged("moz")
        assertEquals(5, suggestions.size)

        provider.setMaxNumberOfSuggestions(45)
        suggestions = provider.onInputChanged("moz")
        assertEquals(45, suggestions.size)

        provider.resetToDefaultMaxSuggestions()
        suggestions = provider.onInputChanged("moz")
        assertEquals(5, suggestions.size)
    }
}
