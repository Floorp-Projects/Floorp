/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.storage.memory.InMemoryHistoryStorage
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.RedirectSource
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class CombinedHistorySuggestionProviderTest {
    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular,
        previewImageUrl = null
    )

    @Test
    fun `GIVEN history items exists WHEN onInputChanged is called with empty text THEN return empty suggestions list`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.mozilla.com", PageVisit(VisitType.TYPED, RedirectSource.NOT_A_SOURCE))
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())

        assertTrue(provider.onInputChanged("").isEmpty())
        assertTrue(provider.onInputChanged("  ").isEmpty())
    }

    @Test
    fun `GIVEN more suggestions asked than metadata items exist WHEN user changes input THEN return a combined list of suggestions`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.mozilla.com/firefox", PageVisit(VisitType.TYPED, RedirectSource.NOT_A_SOURCE))
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())

        val result = provider.onInputChanged("moz")

        assertEquals(2, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
        assertEquals("http://www.mozilla.com/firefox", result[1].description)
    }

    @Test
    fun `GIVEN fewer suggestions asked than metadata items exist WHEN user changes input THEN return suggestions only based on metadata items`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.mozilla.com/firefox", PageVisit(VisitType.TYPED, RedirectSource.NOT_A_SOURCE))
        val provider = CombinedHistorySuggestionProvider(history, storage, mock(), maxNumberOfSuggestions = 1)

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
    }

    @Test
    fun `GIVEN only storage history items exist WHEN user changes input THEN return suggestions only based on storage items`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.mozilla.com/firefox", PageVisit(VisitType.TYPED, RedirectSource.NOT_A_SOURCE))
        val provider = CombinedHistorySuggestionProvider(history, storage, mock(), maxNumberOfSuggestions = 1)

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com/firefox", result[0].description)
    }

    @Test
    fun `GIVEN duplicated metadata and storage entries WHEN user changes input THEN return distinct suggestions`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(eq("moz"), anyInt())
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.mozilla.com", PageVisit(VisitType.TYPED, RedirectSource.NOT_A_SOURCE))
        val provider = CombinedHistorySuggestionProvider(history, storage, mock())

        val result = provider.onInputChanged("moz")

        assertEquals(1, result.size)
        assertEquals("http://www.mozilla.com", result[0].description)
    }
}
