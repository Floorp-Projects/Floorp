/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.browser.storage.memory.InMemoryHistoryStorage
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryStorageSuggestionProviderTest {
    @Test
    fun `Provider returns empty list when text is empty`() = runBlocking {
        val provider = HistoryStorageSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns suggestions from configured history storage`() = runBlocking {
        val history = InMemoryHistoryStorage()
        val provider = HistoryStorageSuggestionProvider(history, mock())

        history.recordVisit("http://www.mozilla.com", VisitType.TYPED)

        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals("http://www.mozilla.com", suggestions[0].description)
    }

    @Test
    fun `Provider limits number of returned suggestions`() = runBlocking {
        val history = InMemoryHistoryStorage()
        val provider = HistoryStorageSuggestionProvider(history, mock())

        for (i in 1..100) {
            history.recordVisit("http://www.mozilla.com/$i", VisitType.TYPED)
        }

        val suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)
    }
}
