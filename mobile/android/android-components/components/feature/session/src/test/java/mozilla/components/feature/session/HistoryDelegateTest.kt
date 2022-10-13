/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class HistoryDelegateTest {

    @Test
    fun `history delegate passes through onVisited calls`() = runTest {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        delegate.onVisited("http://www.mozilla.org", PageVisit(VisitType.LINK))
        verify(storage).recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))

        delegate.onVisited("http://www.firefox.com", PageVisit(VisitType.RELOAD))
        verify(storage).recordVisit("http://www.firefox.com", PageVisit(VisitType.RELOAD))

        delegate.onVisited("http://www.firefox.com", PageVisit(VisitType.BOOKMARK))
        verify(storage).recordVisit("http://www.firefox.com", PageVisit(VisitType.BOOKMARK))
    }

    @Test
    fun `history delegate passes through onTitleChanged calls`() = runTest {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla")
        verify(storage).recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
    }

    @Test
    fun `history delegate passes through onPreviewImageChange calls`() = runTest {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        val previewImageUrl = "https://test.com/og-image-url"
        delegate.onPreviewImageChange("http://www.mozilla.org", previewImageUrl)
        verify(storage).recordObservation(
            "http://www.mozilla.org",
            PageObservation(previewImageUrl = previewImageUrl),
        )
    }

    @Test
    fun `history delegate passes through getVisited calls`() = runTest {
        val storage = TestHistoryStorage()
        val delegate = HistoryDelegate(lazy { storage })

        assertFalse(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)
        assertFalse(storage.canAddUriCalled)

        delegate.getVisited()
        assertTrue(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)
        assertFalse(storage.canAddUriCalled)

        delegate.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))
        assertTrue(storage.getVisitedListCalled)
        assertFalse(storage.canAddUriCalled)
    }

    @Test
    fun `history delegate checks with storage canAddUriCalled`() = runTest {
        val storage = TestHistoryStorage()
        val delegate = HistoryDelegate(lazy { storage })

        assertFalse(storage.canAddUriCalled)
        delegate.shouldStoreUri("test")
        assertTrue(storage.canAddUriCalled)
    }

    private class TestHistoryStorage : HistoryStorage {
        var getVisitedListCalled = false
        var getVisitedPlainCalled = false
        var canAddUriCalled = false

        override suspend fun warmUp() {
            fail()
        }

        override suspend fun recordVisit(uri: String, visit: PageVisit) {}

        override suspend fun recordObservation(uri: String, observation: PageObservation) {}

        override fun canAddUri(uri: String): Boolean {
            canAddUriCalled = true
            return true
        }

        override suspend fun getVisited(uris: List<String>): List<Boolean> {
            getVisitedListCalled = true
            assertEquals(listOf("http://www.mozilla.org", "http://www.firefox.com"), uris)
            return emptyList()
        }

        override suspend fun getVisited(): List<String> {
            getVisitedPlainCalled = true
            return emptyList()
        }

        override suspend fun getDetailedVisits(start: Long, end: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
            fail()
            return emptyList()
        }

        override suspend fun getVisitsPaginated(offset: Long, count: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
            fail()
            return emptyList()
        }

        override suspend fun getTopFrecentSites(
            numItems: Int,
            frecencyThreshold: FrecencyThresholdOption,
        ): List<TopFrecentSiteInfo> {
            fail()
            return emptyList()
        }

        override fun getSuggestions(query: String, limit: Int): List<SearchResult> {
            fail()
            return listOf()
        }

        override fun getAutocompleteSuggestion(query: String): HistoryAutocompleteResult? {
            fail()
            return null
        }

        override suspend fun deleteEverything() {
            fail()
        }

        override suspend fun deleteVisitsSince(since: Long) {
            fail()
        }

        override suspend fun deleteVisitsBetween(startTime: Long, endTime: Long) {
            fail()
        }

        override suspend fun deleteVisitsFor(url: String) {
            fail()
        }

        override suspend fun deleteVisit(url: String, timestamp: Long) {
            fail()
        }

        override suspend fun prune() {
            fail()
        }

        override suspend fun runMaintenance() {
            fail()
        }

        override fun cleanup() {
            fail()
        }

        override fun cancelWrites() {
            fail()
        }

        override fun cancelReads() {
            fail()
        }
    }
}
