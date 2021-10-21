/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import kotlinx.coroutines.runBlocking
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
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class HistoryMetadataSuggestionProviderTest {
    private val historyEntry = HistoryMetadata(
        key = HistoryMetadataKey("http://www.mozilla.com", null, null),
        title = "mozilla",
        createdAt = System.currentTimeMillis(),
        updatedAt = System.currentTimeMillis(),
        totalViewTime = 10,
        documentType = DocumentType.Regular,
        previewImageUrl = null
    )

    @Before
    fun setup() {
        Facts.clearProcessors()
    }

    @Test
    fun `provider returns empty list when text is empty`() = runBlocking {
        val provider = HistoryMetadataSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `provider returns suggestions from configured history storage`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()

        whenever(storage.queryHistoryMetadata("moz", DEFAULT_METADATA_SUGGESTION_LIMIT)).thenReturn(listOf(historyEntry))

        val provider = HistoryMetadataSuggestionProvider(storage, mock())
        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals(historyEntry.key.url, suggestions[0].description)
        assertEquals(historyEntry.title, suggestions[0].title)
    }

    @Test
    fun `provider limits number of returned suggestions to 5 by default`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(storage, mock())

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 5)
        Unit
    }

    @Test
    fun `provider allows lowering the number of returned suggestions beneath the default`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage, loadUrlUseCase = mock(), maxNumberOfSuggestions = 2
        )

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 2)
        Unit
    }

    @Test
    fun `provider allows increasing the number of returned suggestions above the default`() = runBlocking {
        val storage: HistoryMetadataStorage = mock()
        doReturn(emptyList<HistoryMetadata>()).`when`(storage).queryHistoryMetadata(anyString(), anyInt())
        val provider = HistoryMetadataSuggestionProvider(
            historyStorage = storage, loadUrlUseCase = mock(), maxNumberOfSuggestions = 8
        )

        provider.onInputChanged("moz")

        verify(storage).queryHistoryMetadata("moz", 8)
        Unit
    }

    @Test
    fun `provider only as suggestions pages on which users actually spent some time`() = runBlocking {
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
    fun `provider calls speculative connect for URL of highest scored suggestion`() = runBlocking {
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
    fun `fact is emitted when suggestion is clicked`() = runBlocking {
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
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                emittedFacts.add(fact)
            }
        })

        suggestions[0].onSuggestionClicked?.invoke()
        assertTrue(emittedFacts.isNotEmpty())
        assertEquals(
            Fact(
                Component.FEATURE_AWESOMEBAR,
                Action.INTERACTION,
                AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED
            ),
            emittedFacts.first()
        )
    }
}
