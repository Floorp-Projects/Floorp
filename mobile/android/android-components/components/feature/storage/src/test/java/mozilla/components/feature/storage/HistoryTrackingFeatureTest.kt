/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.storage

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.never

class HistoryTrackingFeatureTest {
    @Test
    fun `feature sets a history delegate on the engine`() {
        val engine: Engine = mock()
        val settings = DefaultSettings()
        `when`(engine.settings).thenReturn(settings)

        assertNull(settings.historyTrackingDelegate)
        HistoryTrackingFeature(engine, mock())
        assertNotNull(settings.historyTrackingDelegate)
    }

    @Test
    fun `history delegate doesn't interact with storage in private mode`() = runBlocking {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onVisited("http://www.mozilla.org", false, true)
        verify(storage, never()).recordVisit(any(), any())

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla", true)
        verify(storage, never()).recordObservation(any(), any())

        assertEquals(0, delegate.getVisited(true).await().size)
        assertEquals(listOf(false), delegate.getVisited(listOf("http://www.mozilla.com"), true).await())
        assertEquals(listOf(false, false), delegate.getVisited(listOf("http://www.mozilla.com", "http://www.firefox.com"), true).await())
    }

    @Test
    fun `history delegate passes through onVisited calls`() = runBlocking {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onVisited("http://www.mozilla.org", false, false)
        verify(storage).recordVisit("http://www.mozilla.org", VisitType.LINK)

        delegate.onVisited("http://www.firefox.com", true, false)
        verify(storage).recordVisit("http://www.firefox.com", VisitType.RELOAD)
    }

    @Test
    fun `history delegate passes through onTitleChanged calls`() = runBlocking {
        val storage: HistoryStorage = mock()
        val delegate = HistoryDelegate(storage)

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla", false)
        verify(storage).recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
    }

    @Test
    fun `history delegate passes through getVisited calls`() = runBlocking {
        class TestHistoryStorage : HistoryStorage {
            var getVisitedListCalled = false
            var getVisitedPlainCalled = false

            override suspend fun recordVisit(uri: String, visitType: VisitType) {
                fail()
            }

            override suspend fun recordObservation(uri: String, observation: PageObservation) {
                fail()
            }

            override fun getVisited(uris: List<String>): Deferred<List<Boolean>> {
                getVisitedListCalled = true
                assertEquals(listOf("http://www.mozilla.org", "http://www.firefox.com"), uris)
                return CompletableDeferred(listOf())
            }

            override fun getVisited(): Deferred<List<String>> {
                getVisitedPlainCalled = true
                return CompletableDeferred(listOf())
            }

            override fun getSuggestions(query: String, limit: Int): List<SearchResult> {
                fail()
                return listOf()
            }

            override fun getDomainSuggestion(query: String): String? {
                fail()
                return null
            }

            override fun cleanup() {
                fail()
            }
        }
        val storage = TestHistoryStorage()
        val delegate = HistoryDelegate(storage)

        assertFalse(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)

        delegate.getVisited(false).await()
        assertTrue(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)

        delegate.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"), false).await()
        assertTrue(storage.getVisitedListCalled)
    }
}