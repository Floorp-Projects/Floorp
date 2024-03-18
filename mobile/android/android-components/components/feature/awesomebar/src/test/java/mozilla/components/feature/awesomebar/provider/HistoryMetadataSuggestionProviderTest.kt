/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class HistoryMetadataSuggestionProviderTest {
    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular,
        previewImageUrl = null,
    )

    @Before
    fun setup() {
        Facts.clearProcessors()
    }

    @Test
    fun `provider returns empty list when text is empty`() = runTest {
        val provider = HistoryMetadataSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `provider cleanups all previous read operations when text is empty`() = runTest {
        val provider = HistoryMetadataSuggestionProvider(mock(), mock())

        provider.onInputChanged("")

        verify(provider.historyStorage, never()).cancelReads()
        verify(provider.historyStorage).cancelReads("")
    }

    @Test
    fun `provider cleanups all previous read operations when text is not empty`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(listOf(historyEntry)).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(storage, mock())
        val orderVerifier = inOrder(storage)

        provider.onInputChanged("moz")

        orderVerifier.verify(provider.historyStorage, never()).cancelReads()
        orderVerifier.verify(provider.historyStorage).cancelReads("moz")
        orderVerifier.verify(provider.historyStorage).queryHistoryMetadata(eq("moz"), anyInt())
    }

    @Test
    fun `provider returns suggestions from configured history storage`() = runTest {
        val storage: HistoryMetadataStorage = mock()

        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))

        val provider = HistoryMetadataSuggestionProvider(storage, mock())
        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals(historyEntry.key.url, suggestions[0].description)
        assertEquals(historyEntry.title, suggestions[0].title)
    }

    @Test
    fun `provider limits number of returned suggestions to 5 by default`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(storage, mock())

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 5)
        Unit
    }

    @Test
    fun `provider allows lowering the number of returned suggestions beneath the default`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 2,
        )

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 2)
        Unit
    }

    @Test
    fun `provider allows increasing the number of returned suggestions above the default`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 8,
        )

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 8)
        Unit
    }

    @Test
    fun `provider only as suggestions pages on which users actually spent some time`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        val historyEntries = mutableListOf<HistoryMetadata>().apply {
            add(historyEntry)
            add(historyEntry.copy(totalViewTime = 0))
        }
        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(historyEntries)
        val provider = HistoryMetadataSuggestionProvider(storage, mock())

        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
    }

    @Test
    fun `provider calls speculative connect for URL of highest scored suggestion`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        val engine: Engine = mock()
        val provider = HistoryMetadataSuggestionProvider(storage, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))

        suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        verify(engine, times(1)).speculativeConnect(historyEntry.key.url)
    }

    @Test
    fun `fact is emitted when suggestion is clicked`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        val engine: Engine = mock()
        val provider = HistoryMetadataSuggestionProvider(storage, mock(), engine = engine)

        var suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
        verify(engine, never()).speculativeConnect(anyString())

        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))

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

    @Test
    fun `WHEN provider is set to not show edit suggestions THEN edit suggestion is set to null`() = runTest {
        val storage: HistoryMetadataStorage = mock()

        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))

        val provider = HistoryMetadataSuggestionProvider(storage, mock(), showEditSuggestion = false)
        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals(historyEntry.key.url, suggestions[0].description)
        assertEquals(historyEntry.title, suggestions[0].title)
        assertNull(suggestions[0].editSuggestion)
    }

    @Test
    fun `GIVEN no external filter WHEN querying history THEN query a low number of results`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(storage, mock())

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)
    }

    @Test
    fun `GIVEN a results host filter WHEN querying history THEN query more than the usual default results for the host url`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val expectedQueryCount = 2 * HISTORY_METADATA_RESULTS_TO_FILTER_SCALE_FACTOR

        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage,
            loadUrlUseCase = mock(),
            maxNumberOfSuggestions = 2,
            resultsUriFilter = "test".toUri(),
        )

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", expectedQueryCount)
    }

    @Test
    fun `GIVEN a results host filter WHEN querying history THEN return only the results that pass through the filter`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        val metadataKey2 = HistoryMetadataKey("https://mozilla.com/firefox", null, null)
        val historyEntry2 = HistoryMetadata(
            key = metadataKey2,
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        doReturn(listOf(historyEntry, historyEntry2)).`when`(storage).queryHistoryMetadata(anyString(), anyInt())

        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage,
            loadUrlUseCase = mock(),
            resultsUriFilter = "https://mozilla.com".toUri(),
        )

        val suggestions = provider.onInputChanged("moz")

        assertEquals(2, suggestions.size)
        assertTrue(suggestions.map { it.description }.contains("https://mozilla.com/firefox"))
        assertTrue(suggestions.map { it.description }.contains("http://www.mozilla.com"))
    }

    @Test
    fun `GIVEN a results host filter WHEN querying history THEN return results containing mobile subdomains`() = runTest {
        val storage: HistoryMetadataStorage = mock()
        val metadataKey1 = HistoryMetadataKey("https://m.mozilla.com/firefox", null, null)
        val historyEntry1 = HistoryMetadata(
            key = metadataKey1,
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )

        val metadataKey2 = HistoryMetadataKey("http://www.mobile.mozilla.com/firefox", null, null)
        val historyEntry2 = HistoryMetadata(
            key = metadataKey2,
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular,
            previewImageUrl = null,
        )
        doReturn(listOf(historyEntry1, historyEntry2)).`when`(storage).queryHistoryMetadata(anyString(), anyInt())

        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage,
            loadUrlUseCase = mock(),
            resultsUriFilter = "https://mozilla.com".toUri(),
        )

        val suggestions = provider.onInputChanged("moz")

        assertEquals(2, suggestions.size)
        assertTrue(suggestions.map { it.description }.contains("http://www.mobile.mozilla.com/firefox"))
        assertTrue(suggestions.map { it.description }.contains("https://m.mozilla.com/firefox"))
    }
}
