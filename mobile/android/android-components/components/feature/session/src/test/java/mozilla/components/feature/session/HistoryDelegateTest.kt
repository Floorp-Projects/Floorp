/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
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
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class HistoryDelegateTest {

    @Test
    fun `history delegate passes through onVisited calls`() = runBlocking {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        delegate.onVisited("about:blank", PageVisit(VisitType.TYPED))
        verify(storage, never()).recordVisit("about:blank", PageVisit(VisitType.TYPED))

        delegate.onVisited("http://www.mozilla.org", PageVisit(VisitType.LINK))
        verify(storage).recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))

        delegate.onVisited("http://www.firefox.com", PageVisit(VisitType.RELOAD))
        verify(storage).recordVisit("http://www.firefox.com", PageVisit(VisitType.RELOAD))

        delegate.onVisited("http://www.firefox.com", PageVisit(VisitType.BOOKMARK))
        verify(storage).recordVisit("http://www.firefox.com", PageVisit(VisitType.BOOKMARK))
    }

    @Test
    fun `history delegate passes through onTitleChanged calls`() = runBlocking {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        delegate.onTitleChanged("http://www.mozilla.org", "Mozilla")
        verify(storage).recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))

        delegate.onTitleChanged("about:blank", "Blank!")
        verify(storage, never()).recordObservation("about:blank", PageObservation("Blank!"))
    }

    @Test
    fun `history delegate passes through onPreviewImageChange calls`() = runBlocking {
        val storage = mock<HistoryStorage>()
        val delegate = HistoryDelegate(lazy { storage })

        val previewImageUrl = "https://test.com/og-image-url"
        delegate.onPreviewImageChange("http://www.mozilla.org", previewImageUrl)
        verify(storage).recordObservation(
            "http://www.mozilla.org",
            PageObservation(previewImageUrl = previewImageUrl)
        )

        delegate.onPreviewImageChange("about:blank", previewImageUrl)
        verify(storage, never()).recordObservation(
            "about:blank",
            PageObservation(previewImageUrl = previewImageUrl)
        )
    }

    @Test
    fun `history delegate passes through getVisited calls`() = runBlocking {
        class TestHistoryStorage : HistoryStorage {
            var getVisitedListCalled = false
            var getVisitedPlainCalled = false

            override suspend fun warmUp() {
                fail()
            }

            override suspend fun recordVisit(uri: String, visit: PageVisit) {
                fail()
            }

            override suspend fun recordObservation(uri: String, observation: PageObservation) {
                fail()
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
                frecencyThreshold: FrecencyThresholdOption
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
        }

        val storage = TestHistoryStorage()
        val delegate = HistoryDelegate(lazy { storage })

        assertFalse(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)

        delegate.getVisited()
        assertTrue(storage.getVisitedPlainCalled)
        assertFalse(storage.getVisitedListCalled)

        delegate.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))
        assertTrue(storage.getVisitedListCalled)
    }

    @Test
    fun `history delegate's shouldStoreUri works as expected`() {
        val delegate = HistoryDelegate(mock())

        // Not an excessive list of allowed schemes.
        assertTrue(delegate.shouldStoreUri("http://www.mozilla.com"))
        assertTrue(delegate.shouldStoreUri("https://www.mozilla.com"))
        assertTrue(delegate.shouldStoreUri("ftp://files.mozilla.com/stuff/fenix.apk"))
        assertTrue(delegate.shouldStoreUri("about:reader?url=http://www.mozilla.com/interesting-article.html"))
        assertTrue(delegate.shouldStoreUri("https://john.doe@www.example.com:123/forum/questions/?tag=networking&order=newest#top"))
        assertTrue(delegate.shouldStoreUri("ldap://2001:db8::7/c=GB?objectClass?one"))
        assertTrue(delegate.shouldStoreUri("telnet://192.0.2.16:80/"))

        assertFalse(delegate.shouldStoreUri("withoutSchema.html"))
        assertFalse(delegate.shouldStoreUri("about:blank"))
        assertFalse(delegate.shouldStoreUri("news:comp.infosystems.www.servers.unix"))
        assertFalse(delegate.shouldStoreUri("imap://mail.example.com/~mozilla"))
        assertFalse(delegate.shouldStoreUri("chrome://config"))
        assertFalse(delegate.shouldStoreUri("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D"))
        assertFalse(delegate.shouldStoreUri("data:text/html,<script>alert('hi');</script>"))
        assertFalse(delegate.shouldStoreUri("resource://internal-thingy-js-inspector/script.js"))
        assertFalse(delegate.shouldStoreUri("javascript:alert('hello!');"))
        assertFalse(delegate.shouldStoreUri("blob:https://api.mozilla.com/resource.png"))
    }
}
