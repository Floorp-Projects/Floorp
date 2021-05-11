/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class HistoryMetadataSuggestionProviderTest {

    @Test
    fun `provider returns empty list when text is empty`() = runBlocking {
        val provider = HistoryMetadataSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `provider returns suggestions from configured history storage`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()

        val result = HistoryMetadata(
            guid = "testGuid",
            url = "http://www.mozilla.com",
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            isMedia = false,
            parentUrl = null,
            searchTerm = null
        )
        whenever(storage.queryHistoryMetadata("moz", METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(result))

        val provider = HistoryMetadataSuggestionProvider(storage, mock())
        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals(result.url, suggestions[0].description)
        assertEquals(result.title, suggestions[0].title)
        assertEquals(result.guid, suggestions[0].id)
    }

    @Test
    fun `provider suggestion should not get cleared when text changes`() {
        val provider = HistoryMetadataSuggestionProvider(mock(), mock())
        assertFalse(provider.shouldClearSuggestions)
    }

    @Test
    fun `provider calls speculative connect for URL of highest scored suggestion`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        val engine: Engine = mock()
        val provider = HistoryMetadataSuggestionProvider(storage, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        val result = HistoryMetadata(
            guid = "testGuid",
            url = "http://www.mozilla.com",
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            isMedia = false,
            parentUrl = null,
            searchTerm = null
        )
        whenever(storage.queryHistoryMetadata("moz", METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(result))

        suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        verify(engine, times(1)).speculativeConnect(result.url)
    }
}
