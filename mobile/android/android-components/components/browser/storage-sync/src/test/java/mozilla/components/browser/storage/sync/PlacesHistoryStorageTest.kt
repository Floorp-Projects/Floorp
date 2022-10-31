/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.appservices.places.uniffi.PlacesException
import mozilla.appservices.places.uniffi.VisitObservation
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import java.io.File

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class PlacesHistoryStorageTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var history: PlacesHistoryStorage

    @Before
    fun setup() = runTestOnMain {
        history = PlacesHistoryStorage(testContext, mock())
        // There's a database on disk which needs to be cleaned up between tests.
        history.deleteEverything()
    }

    @After
    @Suppress("DEPRECATION")
    fun cleanup() = runTestOnMain {
        history.cleanup()
    }

    @Test
    fun `storage allows recording and querying visits of different types`() = runTestOnMain {
        history.recordVisit("http://www.firefox.com/1", PageVisit(VisitType.LINK))
        history.recordVisit("http://www.firefox.com/2", PageVisit(VisitType.RELOAD))
        history.recordVisit("http://www.firefox.com/3", PageVisit(VisitType.TYPED))
        history.recordVisit("http://www.firefox.com/4", PageVisit(VisitType.REDIRECT_TEMPORARY))
        history.recordVisit("http://www.firefox.com/5", PageVisit(VisitType.REDIRECT_PERMANENT))
        history.recordVisit("http://www.firefox.com/6", PageVisit(VisitType.FRAMED_LINK))
        history.recordVisit("http://www.firefox.com/7", PageVisit(VisitType.EMBED))
        history.recordVisit("http://www.firefox.com/8", PageVisit(VisitType.BOOKMARK))
        history.recordVisit("http://www.firefox.com/9", PageVisit(VisitType.DOWNLOAD))

        val recordedVisits = history.getDetailedVisits(0)
        assertEquals(9, recordedVisits.size)
        assertEquals("http://www.firefox.com/1", recordedVisits[0].url)
        assertEquals(VisitType.LINK, recordedVisits[0].visitType)
        assertEquals("http://www.firefox.com/2", recordedVisits[1].url)
        assertEquals(VisitType.RELOAD, recordedVisits[1].visitType)
        assertEquals("http://www.firefox.com/3", recordedVisits[2].url)
        assertEquals(VisitType.TYPED, recordedVisits[2].visitType)
        assertEquals("http://www.firefox.com/4", recordedVisits[3].url)
        assertEquals(VisitType.REDIRECT_TEMPORARY, recordedVisits[3].visitType)
        assertEquals("http://www.firefox.com/5", recordedVisits[4].url)
        assertEquals(VisitType.REDIRECT_PERMANENT, recordedVisits[4].visitType)
        assertEquals("http://www.firefox.com/6", recordedVisits[5].url)
        assertEquals(VisitType.FRAMED_LINK, recordedVisits[5].visitType)
        assertEquals("http://www.firefox.com/7", recordedVisits[6].url)
        assertEquals(VisitType.EMBED, recordedVisits[6].visitType)
        assertEquals("http://www.firefox.com/8", recordedVisits[7].url)
        assertEquals(VisitType.BOOKMARK, recordedVisits[7].visitType)
        assertEquals("http://www.firefox.com/9", recordedVisits[8].url)
        assertEquals(VisitType.DOWNLOAD, recordedVisits[8].visitType)

        // Can use WebView-style getVisited API.
        assertEquals(
            listOf(
                "http://www.firefox.com/1", "http://www.firefox.com/2", "http://www.firefox.com/3",
                "http://www.firefox.com/4", "http://www.firefox.com/5", "http://www.firefox.com/6",
                "http://www.firefox.com/7", "http://www.firefox.com/8", "http://www.firefox.com/9",
            ),
            history.getVisited(),
        )

        // Can use GeckoView-style getVisited API.
        assertEquals(
            listOf(false, true, true, true, true, true, true, false, true, true, true),
            history.getVisited(
                listOf(
                    "http://www.mozilla.com",
                    "http://www.firefox.com/1", "http://www.firefox.com/2", "http://www.firefox.com/3",
                    "http://www.firefox.com/4", "http://www.firefox.com/5", "http://www.firefox.com/6",
                    "http://www.firefox.com/oops",
                    "http://www.firefox.com/7", "http://www.firefox.com/8", "http://www.firefox.com/9",
                ),
            ),
        )

        // Can query using pagination.
        val page1 = history.getVisitsPaginated(0, 3)
        assertEquals(3, page1.size)
        assertEquals("http://www.firefox.com/9", page1[0].url)
        assertEquals("http://www.firefox.com/8", page1[1].url)
        assertEquals("http://www.firefox.com/7", page1[2].url)

        // Can exclude visit types during pagination.
        val page1Limited = history.getVisitsPaginated(0, 10, listOf(VisitType.REDIRECT_PERMANENT, VisitType.REDIRECT_TEMPORARY))
        assertEquals(7, page1Limited.size)
        assertEquals("http://www.firefox.com/9", page1Limited[0].url)
        assertEquals("http://www.firefox.com/8", page1Limited[1].url)
        assertEquals("http://www.firefox.com/7", page1Limited[2].url)
        assertEquals("http://www.firefox.com/6", page1Limited[3].url)
        assertEquals("http://www.firefox.com/3", page1Limited[4].url)
        assertEquals("http://www.firefox.com/2", page1Limited[5].url)
        assertEquals("http://www.firefox.com/1", page1Limited[6].url)

        val page2 = history.getVisitsPaginated(3, 3)
        assertEquals(3, page2.size)
        assertEquals("http://www.firefox.com/6", page2[0].url)
        assertEquals("http://www.firefox.com/5", page2[1].url)
        assertEquals("http://www.firefox.com/4", page2[2].url)

        val page3 = history.getVisitsPaginated(6, 10)
        assertEquals(3, page3.size)
        assertEquals("http://www.firefox.com/3", page3[0].url)
        assertEquals("http://www.firefox.com/2", page3[1].url)
        assertEquals("http://www.firefox.com/1", page3[2].url)
    }

    @Test
    fun `storage passes through recordObservation calls`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))
        history.recordObservation("http://www.mozilla.org", PageObservation(title = "Mozilla"))

        var recordedVisits = history.getDetailedVisits(0)
        assertEquals(1, recordedVisits.size)
        assertEquals("Mozilla", recordedVisits[0].title)
        assertNull(recordedVisits[0].previewImageUrl)

        history.recordObservation("http://www.mozilla.org", PageObservation(previewImageUrl = "https://test.com/og-image-url"))

        recordedVisits = history.getDetailedVisits(0)
        assertEquals(1, recordedVisits.size)
        assertEquals("Mozilla", recordedVisits[0].title)
        assertEquals("https://test.com/og-image-url", recordedVisits[0].previewImageUrl)
    }

    @Test
    fun `store can be used to query top frecent site information`() = runTestOnMain {
        val toAdd = listOf(
            "https://www.example.com/123",
            "https://www.example.com/123",
            "https://www.example.com/12345",
            "https://www.mozilla.com/foo/bar/baz",
            "https://www.mozilla.com/foo/bar/baz",
            "https://mozilla.com/a1/b2/c3",
            "https://news.ycombinator.com/",
            "https://www.mozilla.com/foo/bar/baz",
        )

        for (url in toAdd) {
            history.recordVisit(url, PageVisit(VisitType.LINK))
        }

        var infos = history.getTopFrecentSites(0, frecencyThreshold = FrecencyThresholdOption.NONE)
        assertEquals(0, infos.size)

        infos = history.getTopFrecentSites(0, frecencyThreshold = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES)
        assertEquals(0, infos.size)

        infos = history.getTopFrecentSites(3, frecencyThreshold = FrecencyThresholdOption.NONE)
        assertEquals(3, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)
        assertEquals("https://news.ycombinator.com/", infos[2].url)

        infos = history.getTopFrecentSites(3, frecencyThreshold = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES)
        assertEquals(2, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)

        infos = history.getTopFrecentSites(5, frecencyThreshold = FrecencyThresholdOption.NONE)
        assertEquals(5, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)
        assertEquals("https://news.ycombinator.com/", infos[2].url)
        assertEquals("https://mozilla.com/a1/b2/c3", infos[3].url)
        assertEquals("https://www.example.com/12345", infos[4].url)

        infos = history.getTopFrecentSites(5, frecencyThreshold = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES)
        assertEquals(2, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)

        infos = history.getTopFrecentSites(100, frecencyThreshold = FrecencyThresholdOption.NONE)
        assertEquals(5, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)
        assertEquals("https://news.ycombinator.com/", infos[2].url)
        assertEquals("https://mozilla.com/a1/b2/c3", infos[3].url)
        assertEquals("https://www.example.com/12345", infos[4].url)

        infos = history.getTopFrecentSites(100, frecencyThreshold = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES)
        assertEquals(2, infos.size)
        assertEquals("https://www.mozilla.com/foo/bar/baz", infos[0].url)
        assertEquals("https://www.example.com/123", infos[1].url)

        infos = history.getTopFrecentSites(-4, frecencyThreshold = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES)
        assertEquals(0, infos.size)
    }

    @Test
    fun `store can be used to query detailed visit information`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.RELOAD))
        history.recordObservation(
            "http://www.mozilla.org",
            PageObservation("Mozilla", "https://test.com/og-image-url"),
        )
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.LINK))

        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.REDIRECT_TEMPORARY))

        val visits = history.getDetailedVisits(0, excludeTypes = listOf(VisitType.REDIRECT_TEMPORARY))
        assertEquals(3, visits.size)
        assertEquals("http://www.mozilla.org/", visits[0].url)
        assertEquals("Mozilla", visits[0].title)
        assertEquals("https://test.com/og-image-url", visits[0].previewImageUrl)
        assertEquals(VisitType.LINK, visits[0].visitType)

        assertEquals("http://www.mozilla.org/", visits[1].url)
        assertEquals("Mozilla", visits[1].title)
        assertEquals(VisitType.RELOAD, visits[1].visitType)

        assertEquals("http://www.firefox.com/", visits[2].url)
        assertEquals(null, visits[2].title)
        assertEquals(VisitType.LINK, visits[2].visitType)

        val visitsAll = history.getDetailedVisits(0)
        assertEquals(4, visitsAll.size)
    }

    @Test
    fun `store can be used to record and retrieve history via webview-style callbacks`() = runTestOnMain {
        // Empty.
        assertEquals(0, history.getVisited().size)

        // Regular visits are tracked.
        history.recordVisit("https://www.mozilla.org", PageVisit(VisitType.LINK))
        assertEquals(listOf("https://www.mozilla.org/"), history.getVisited())

        // Multiple visits can be tracked, results ordered by "URL's first seen first".
        history.recordVisit("https://www.firefox.com", PageVisit(VisitType.LINK))
        assertEquals(listOf("https://www.mozilla.org/", "https://www.firefox.com/"), history.getVisited())

        // Visits marked as reloads can be tracked.
        history.recordVisit("https://www.firefox.com", PageVisit(VisitType.RELOAD))
        assertEquals(listOf("https://www.mozilla.org/", "https://www.firefox.com/"), history.getVisited())

        // Visited urls are certainly a set.
        history.recordVisit("https://www.firefox.com", PageVisit(VisitType.LINK))
        history.recordVisit("https://www.mozilla.org", PageVisit(VisitType.LINK))
        history.recordVisit("https://www.wikipedia.org", PageVisit(VisitType.LINK))
        assertEquals(
            listOf("https://www.mozilla.org/", "https://www.firefox.com/", "https://www.wikipedia.org/"),
            history.getVisited(),
        )
    }

    @Test
    fun `store can be used to record and retrieve history via gecko-style callbacks`() = runTestOnMain {
        assertEquals(0, history.getVisited(listOf()).size)

        // Regular visits are tracked
        history.recordVisit("https://www.mozilla.org", PageVisit(VisitType.LINK))
        assertEquals(listOf(true), history.getVisited(listOf("https://www.mozilla.org")))

        // Duplicate requests are handled.
        assertEquals(listOf(true, true), history.getVisited(listOf("https://www.mozilla.org", "https://www.mozilla.org")))

        // Visit map is returned in correct order.
        assertEquals(listOf(true, false), history.getVisited(listOf("https://www.mozilla.org", "https://www.unknown.com")))

        assertEquals(listOf(false, true), history.getVisited(listOf("https://www.unknown.com", "https://www.mozilla.org")))

        // Multiple visits can be tracked. Reloads can be tracked.
        history.recordVisit("https://www.firefox.com", PageVisit(VisitType.LINK))
        history.recordVisit("https://www.mozilla.org", PageVisit(VisitType.RELOAD))
        history.recordVisit("https://www.wikipedia.org", PageVisit(VisitType.LINK))
        assertEquals(listOf(true, true, false, true), history.getVisited(listOf("https://www.firefox.com", "https://www.wikipedia.org", "https://www.unknown.com", "https://www.mozilla.org")))
    }

    @Test
    fun `store can be used to track page meta information - title and previewImageUrl changes`() = runTestOnMain {
        // Title and previewImageUrl changes are recorded.
        history.recordVisit("https://www.wikipedia.org", PageVisit(VisitType.TYPED))
        history.recordObservation(
            "https://www.wikipedia.org",
            PageObservation("Wikipedia", "https://test.com/og-image-url"),
        )
        var recorded = history.getDetailedVisits(0)
        assertEquals(1, recorded.size)
        assertEquals("Wikipedia", recorded[0].title)
        assertEquals("https://test.com/og-image-url", recorded[0].previewImageUrl)

        history.recordObservation("https://www.wikipedia.org", PageObservation("Википедия"))
        recorded = history.getDetailedVisits(0)
        assertEquals(1, recorded.size)
        assertEquals("Википедия", recorded[0].title)
        assertEquals("https://test.com/og-image-url", recorded[0].previewImageUrl)

        // Titles for different pages are recorded.
        history.recordVisit("https://www.firefox.com", PageVisit(VisitType.TYPED))
        history.recordObservation("https://www.firefox.com", PageObservation("Firefox"))
        history.recordVisit("https://www.mozilla.org", PageVisit(VisitType.TYPED))
        history.recordObservation("https://www.mozilla.org", PageObservation("Мозилла"))
        recorded = history.getDetailedVisits(0)
        assertEquals(3, recorded.size)
        assertEquals("Википедия", recorded[0].title)
        assertEquals("https://test.com/og-image-url", recorded[0].previewImageUrl)
        assertEquals("Firefox", recorded[1].title)
        assertNull(recorded[1].previewImageUrl)
        assertEquals("Мозилла", recorded[2].title)
        assertNull(recorded[2].previewImageUrl)
    }

    @Test
    fun `store can provide suggestions`() = runTestOnMain {
        assertEquals(0, history.getSuggestions("Mozilla", 100).size)

        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.LINK))
        val search = history.getSuggestions("Mozilla", 100)
        assertEquals(0, search.size)

        history.recordVisit("http://www.wikipedia.org", PageVisit(VisitType.LINK))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))
        history.recordVisit("http://www.moscow.ru", PageVisit(VisitType.LINK))
        history.recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
        history.recordObservation("http://www.firefox.com", PageObservation("Mozilla Firefox"))
        history.recordObservation("http://www.moscow.ru", PageObservation("Moscow City"))
        history.recordObservation("http://www.moscow.ru/notitle", PageObservation(""))

        // Empty search.
        assertEquals(4, history.getSuggestions("", 100).size)

        val search2 = history.getSuggestions("Mozilla", 100).sortedByDescending { it.url }
        assertEquals(2, search2.size)
        assertEquals("http://www.mozilla.org/", search2[0].id)
        assertEquals("http://www.mozilla.org/", search2[0].url)
        assertEquals("Mozilla", search2[0].title)
        assertEquals("http://www.firefox.com/", search2[1].id)
        assertEquals("http://www.firefox.com/", search2[1].url)
        assertEquals("Mozilla Firefox", search2[1].title)

        val search3 = history.getSuggestions("Mo", 100).sortedByDescending { it.url }
        assertEquals(3, search3.size)

        assertEquals("http://www.mozilla.org/", search3[0].id)
        assertEquals("http://www.mozilla.org/", search3[0].url)
        assertEquals("Mozilla", search3[0].title)

        assertEquals("http://www.moscow.ru/", search3[1].id)
        assertEquals("http://www.moscow.ru/", search3[1].url)
        assertEquals("Moscow City", search3[1].title)

        assertEquals("http://www.firefox.com/", search3[2].id)
        assertEquals("http://www.firefox.com/", search3[2].url)
        assertEquals("Mozilla Firefox", search3[2].title)

        // Respects the limit
        val search4 = history.getSuggestions("Mo", 1)
        assertEquals("http://www.moscow.ru/", search4[0].id)
        assertEquals("http://www.moscow.ru/", search4[0].url)
        assertEquals("Moscow City", search4[0].title)
    }

    @Test
    fun `store can provide autocomplete suggestions`() = runTestOnMain {
        assertNull(history.getAutocompleteSuggestion("moz"))

        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.LINK))
        var res = history.getAutocompleteSuggestion("moz")!!
        assertEquals("mozilla.org/", res.text)
        assertEquals("http://www.mozilla.org/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        history.recordVisit("http://firefox.com", PageVisit(VisitType.LINK))
        res = history.getAutocompleteSuggestion("firefox")!!
        assertEquals("firefox.com/", res.text)
        assertEquals("http://firefox.com/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        history.recordVisit("https://en.wikipedia.org/wiki/Mozilla", PageVisit(VisitType.LINK))
        res = history.getAutocompleteSuggestion("en")!!
        assertEquals("en.wikipedia.org/", res.text)
        assertEquals("https://en.wikipedia.org/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        res = history.getAutocompleteSuggestion("en.wikipedia.org/wi")!!
        assertEquals("en.wikipedia.org/wiki/", res.text)
        assertEquals("https://en.wikipedia.org/wiki/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        // Path segment matching along a long path
        history.recordVisit("https://www.reddit.com/r/vancouver/comments/quu9lt/hwy_1_just_north_of_lytton_is_gone", PageVisit(VisitType.LINK))
        res = history.getAutocompleteSuggestion("reddit.com/r")!!
        assertEquals("reddit.com/r/", res.text)
        assertEquals("https://www.reddit.com/r/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)
        res = history.getAutocompleteSuggestion("reddit.com/r/van")!!
        assertEquals("reddit.com/r/vancouver/", res.text)
        assertEquals("https://www.reddit.com/r/vancouver/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)
        res = history.getAutocompleteSuggestion("reddit.com/r/vancouver/comments/q")!!
        assertEquals("reddit.com/r/vancouver/comments/quu9lt/", res.text)
        assertEquals("https://www.reddit.com/r/vancouver/comments/quu9lt/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)
        res = history.getAutocompleteSuggestion("reddit.com/r/vancouver/comments/quu9lt/h")!!
        assertEquals("reddit.com/r/vancouver/comments/quu9lt/hwy_1_just_north_of_lytton_is_gone", res.text)
        assertEquals("https://www.reddit.com/r/vancouver/comments/quu9lt/hwy_1_just_north_of_lytton_is_gone", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        assertNull(history.getAutocompleteSuggestion("hello"))
    }

    @Test
    fun `store uses a different reader for autocomplete suggestions`() {
        val connection: RustPlacesConnection = mock()
        doReturn(mock<PlacesReaderConnection>()).`when`(connection).reader()
        doReturn(mock<PlacesReaderConnection>()).`when`(connection).newReader()
        val storage = MockingPlacesHistoryStorage(connection)

        storage.getAutocompleteSuggestion("Test")

        assertNotEquals(storage.reader, storage.autocompleteReader)
        verify(storage.autocompleteReader).interrupt()
        verify(storage.autocompleteReader).matchUrl("Test")
    }

    @Test
    fun `store ignores url parse exceptions during record operations`() = runTestOnMain {
        // These aren't valid URIs, and if we're not explicitly ignoring exceptions from the underlying
        // storage layer, these calls will throw.
        history.recordVisit("mozilla.org", PageVisit(VisitType.LINK))
        history.recordObservation("mozilla.org", PageObservation("mozilla"))
    }

    @Test
    fun `store can delete everything`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.TYPED))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.DOWNLOAD))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.BOOKMARK))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.RELOAD))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.EMBED))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.REDIRECT_PERMANENT))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.REDIRECT_TEMPORARY))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.LINK))

        history.recordObservation("http://www.firefox.com", PageObservation("Firefox"))

        assertEquals(2, history.getVisited().size)

        history.deleteEverything()

        assertEquals(0, history.getVisited().size)
    }

    @Test
    fun `store can delete by url`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.TYPED))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.DOWNLOAD))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.BOOKMARK))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.RELOAD))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.EMBED))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.REDIRECT_PERMANENT))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.REDIRECT_TEMPORARY))
        history.recordVisit("http://www.firefox.com", PageVisit(VisitType.LINK))

        history.recordObservation("http://www.firefox.com", PageObservation("Firefox"))

        assertEquals(2, history.getVisited().size)

        history.deleteVisitsFor("http://www.mozilla.org")

        assertEquals(1, history.getVisited().size)
        assertEquals("http://www.firefox.com/", history.getVisited()[0])

        history.deleteVisitsFor("http://www.firefox.com")
        assertEquals(0, history.getVisited().size)
    }

    @Test
    fun `store can delete by 'since'`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.TYPED))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.DOWNLOAD))
        history.recordVisit("http://www.mozilla.org", PageVisit(VisitType.BOOKMARK))

        history.deleteVisitsSince(0)
        val visits = history.getVisited()
        assertEquals(0, visits.size)
    }

    @Test
    fun `store can delete by 'range'`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org/1", PageVisit(VisitType.TYPED))
        advanceUntilIdle()
        history.recordVisit("http://www.mozilla.org/2", PageVisit(VisitType.DOWNLOAD))
        advanceUntilIdle()
        history.recordVisit("http://www.mozilla.org/3", PageVisit(VisitType.BOOKMARK))

        var visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(3, visits.size)
        val ts = visits[1].visitTime

        history.deleteVisitsBetween(ts - 1, ts + 1)

        visits = history.getDetailedVisits(0, Long.MAX_VALUE)

        assertEquals(2, visits.size)

        assertEquals("http://www.mozilla.org/1", visits[0].url)
        assertEquals("http://www.mozilla.org/3", visits[1].url)
    }

    @Test
    fun `store can delete visit by 'url' and 'timestamp'`() = runTestOnMain {
        history.recordVisit("http://www.mozilla.org/1", PageVisit(VisitType.TYPED))
        Thread.sleep(10)
        history.recordVisit("http://www.mozilla.org/2", PageVisit(VisitType.DOWNLOAD))
        Thread.sleep(10)
        history.recordVisit("http://www.mozilla.org/3", PageVisit(VisitType.BOOKMARK))

        var visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(3, visits.size)
        val ts = visits[1].visitTime

        history.deleteVisit("http://www.mozilla.org/4", 111)
        // There are no visits for this url, delete is a no-op.
        assertEquals(3, history.getDetailedVisits(0, Long.MAX_VALUE).size)

        history.deleteVisit("http://www.mozilla.org/1", ts)
        // There is no such visit for this url, delete is a no-op.
        assertEquals(3, history.getDetailedVisits(0, Long.MAX_VALUE).size)

        history.deleteVisit("http://www.mozilla.org/2", ts)

        visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(2, visits.size)

        assertEquals("http://www.mozilla.org/1", visits[0].url)
        assertEquals("http://www.mozilla.org/3", visits[1].url)
    }

    @Test
    fun `can run maintanence on the store`() = runTestOnMain {
        history.runMaintenance()
    }

    @Test
    fun `can run prune on the store`() = runTestOnMain {
        // Empty.
        history.prune()
        history.recordVisit("http://www.mozilla.org/1", PageVisit(VisitType.TYPED))
        // Non-empty.
        history.prune()
    }

    @Test(expected = IllegalArgumentException::class)
    fun `storage validates calls to getSuggestion`() {
        history.getSuggestions("Hello!", -1)
    }

    // We can't test 'sync' stuff yet, since that exercises the network and we can't mock that out currently.
    // Instead, we test that our wrappers act correctly.
    internal class MockingPlacesHistoryStorage(override val places: Connection) : PlacesHistoryStorage(testContext)

    @Test
    fun `storage passes through sync calls`() = runTestOnMain {
        var passedAuthInfo: SyncAuthInfo? = null
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun newReader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                assertNull(passedAuthInfo)
                passedAuthInfo = syncInfo
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                fail()
            }

            override fun importVisitsFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun importBookmarksFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode> {
                fail()
                return emptyList()
            }

            override fun close() {
                fail()
            }

            override fun registerWithSyncManager() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)

        storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        assertEquals("kid", passedAuthInfo!!.kid)
        assertEquals("serverUrl", passedAuthInfo!!.tokenServerUrl)
        assertEquals("token", passedAuthInfo!!.fxaAccessToken)
        assertEquals(123L, passedAuthInfo!!.fxaAccessTokenExpiresAt)
        assertEquals("key", passedAuthInfo!!.syncKey)
    }

    @Test
    fun `storage passes through sync OK results`() = runTestOnMain {
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun newReader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {}

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {}

            override fun importVisitsFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun importBookmarksFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode> {
                fail()
                return emptyList()
            }

            override fun close() {
                fail()
            }

            override fun registerWithSyncManager() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))
        assertEquals(SyncStatus.Ok, result)
    }

    @Test
    fun `storage passes through sync exceptions`() = runTestOnMain {
        // Can be any PlacesException
        val exception = PlacesException.UrlParseFailed("test error")
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun newReader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                throw exception
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                fail()
            }

            override fun importVisitsFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun importBookmarksFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode> {
                fail()
                return emptyList()
            }

            override fun close() {
                fail()
            }

            override fun registerWithSyncManager() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        assertTrue(result is SyncStatus.Error)

        val error = result as SyncStatus.Error
        assertEquals("test error", error.exception.message)
    }

    @Test(expected = PlacesException::class)
    fun `storage re-throws sync panics`() = runTestOnMain {
        val exception = PlacesException.UnexpectedPlacesException("test panic")
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun newReader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                throw exception
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                fail()
            }

            override fun importVisitsFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun importBookmarksFromFennec(dbPath: String): JSONObject {
                fail()
                return JSONObject()
            }

            override fun readPinnedSitesFromFennec(dbPath: String): List<BookmarkNode> {
                fail()
                return emptyList()
            }

            override fun close() {
                fail()
            }

            override fun registerWithSyncManager() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)
        storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))
        fail()
    }

    @Test
    fun `history import v0 empty`() {
        // Doesn't have a schema or a set user_version pragma.
        val path = getTestPath("databases/empty-v0.db").absolutePath
        try {
            history.importFromFennec(path)
            fail("Expected v0 database to be unsupported")
        } catch (e: PlacesException) {
            // This is a little brittle, but the places library doesn't have a proper error type for this.
            assertEquals("Unexpected error: Can not import from database version 0", e.message)
        }
    }

    @Test
    fun `history import v23 populated`() {
        // Fennec v38 schema populated with data.
        val path = getTestPath("databases/bookmarks-v23.db").absolutePath
        try {
            history.importFromFennec(path)
            fail("Expected v23 database to be unsupported")
        } catch (e: PlacesException) {
            // This is a little brittle, but the places library doesn't have a proper error type for this.
            assertEquals("Unexpected error: Can not import from database version 23", e.message)
        }
    }

    @Test
    fun `history import v38 populated`() = runTestOnMain {
        val path = getTestPath("databases/populated-v38.db").absolutePath
        var visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(0, visits.size)
        history.importFromFennec(path)

        visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(152, visits.size)

        assertEquals(
            listOf(false, false, true, true, false),
            history.reader.getVisited(
                listOf(
                    "files:///",
                    "https://news.ycombinator.com/",
                    "https://www.theguardian.com/film/2017/jul/24/stranger-things-thor-ragnarok-comic-con-2017",
                    "http://www.bbc.com/news/world-us-canada-40662772",
                    "https://mobile.reuters.com/",
                ),
            ),
        )

        with(visits[0]) {
            assertEquals("Apple", this.title)
            assertEquals("http://www.apple.com/", this.url)
            assertEquals(1472685165382, this.visitTime)
            assertEquals(VisitType.REDIRECT_PERMANENT, this.visitType)
        }
    }

    @Test
    fun `history import v39 populated`() = runTestOnMain {
        val path = getTestPath("databases/populated-v39.db").absolutePath
        var visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(0, visits.size)
        history.importFromFennec(path)

        visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(6, visits.size)

        assertEquals(
            listOf(false, true, true, true, true, true, true),
            history.reader.getVisited(
                listOf(
                    "files:///",
                    "https://news.ycombinator.com/",
                    "https://news.ycombinator.com/item?id=21224209",
                    "https://mobile.twitter.com/random_walker/status/1182635589604171776",
                    "https://www.mozilla.org/en-US/",
                    "https://www.mozilla.org/en-US/firefox/accounts/",
                    "https://mobile.reuters.com/",
                ),
            ),
        )

        with(visits[0]) {
            assertEquals("Hacker News", this.title)
            assertEquals("https://news.ycombinator.com/", this.url)
            assertEquals(1570822280639, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[1]) {
            assertEquals("Why Enterprise Software Sucks | Hacker News", this.title)
            assertEquals("https://news.ycombinator.com/item?id=21224209", this.url)
            assertEquals(1570822283117, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[2]) {
            assertEquals("Arvind Narayanan on Twitter: \"My university just announced that it’s dumping Blackboard, and there was much rejoicing. Why is Blackboard universally reviled? There’s a standard story of why \"enterprise software\" sucks. If you’ll bear with me, I think this is best appreciated by talking about… baby clothes!\" / Twitter", this.title)
            assertEquals("https://mobile.twitter.com/random_walker/status/1182635589604171776", this.url)
            assertEquals(1570822287349, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[3]) {
            assertEquals("Internet for people, not profit — Mozilla", this.title)
            assertEquals("https://www.mozilla.org/en-US/", this.url)
            assertEquals(1570830201733, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[4]) {
            assertEquals("There is a way to protect your privacy. Join Firefox.", this.title)
            assertEquals("https://www.mozilla.org/en-US/firefox/accounts/", this.url)
            assertEquals(1570830207742, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[5]) {
            assertEquals("", this.title)
            assertEquals("https://mobile.reuters.com/", this.url)
            assertEquals(1570830217562, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
    }

    @Test
    fun `history import v34 populated`() = runTestOnMain {
        val path = getTestPath("databases/history-v34.db").absolutePath
        var visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(0, visits.size)
        history.importFromFennec(path)

        visits = history.getDetailedVisits(0, Long.MAX_VALUE)
        assertEquals(6, visits.size)

        assertEquals(
            listOf(true, true, true, true, true),
            history.reader.getVisited(
                listOf(
                    "https://www.newegg.com/",
                    "https://news.ycombinator.com/",
                    "https://terrytao.wordpress.com/2020/04/12/john-conway/",
                    "https://news.ycombinator.com/item?id=22862053",
                    "https://malleable.systems/",
                ),
            ),
        )

        with(visits[0]) {
            assertEquals("Computer Parts, PC Components, Laptop Computers, LED LCD TV, Digital Cameras and more - Newegg.com", this.title)
            assertEquals("https://www.newegg.com/", this.url)
            assertEquals(1586838104188, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[1]) {
            assertEquals("Hacker News", this.title)
            assertEquals("https://news.ycombinator.com/", this.url)
            assertEquals(1586838109506, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[2]) {
            assertEquals("https://terrytao.wordpress.com/2020/04/12/john-conway/", this.title)
            assertEquals("https://terrytao.wordpress.com/2020/04/12/john-conway/", this.url)
            assertEquals(1586838113212, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[3]) {
            assertEquals("John Conway | Hacker News", this.title)
            assertEquals("https://news.ycombinator.com/item?id=22862053", this.url)
            assertEquals(1586838123314, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[4]) {
            assertEquals("John Conway | Hacker News", this.title)
            assertEquals("https://news.ycombinator.com/item?id=22862053", this.url)
            assertEquals(1586838126671, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
        with(visits[5]) {
            assertEquals("https://malleable.systems/", this.title)
            assertEquals("https://malleable.systems/", this.url)
            assertEquals(1586838164613, this.visitTime)
            assertEquals(VisitType.LINK, this.visitType)
        }
    }

    @Test
    fun `record and get latest history metadata by url`() = runTestOnMain {
        val metaKey = HistoryMetadataKey(
            url = "https://doc.rust-lang.org/std/macro.assert_eq.html",
            searchTerm = "rust assert_eq",
            referrerUrl = "http://www.google.com/",
        )

        assertNull(history.getLatestHistoryMetadataForUrl(metaKey.url))
        history.noteHistoryMetadataObservation(metaKey, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular))
        history.noteHistoryMetadataObservation(metaKey, HistoryMetadataObservation.ViewTimeObservation(5000))

        val dbMeta = history.getLatestHistoryMetadataForUrl(metaKey.url)
        assertNotNull(dbMeta)
        assertHistoryMetadataRecord(metaKey, 5000, DocumentType.Regular, dbMeta!!)
    }

    @Test
    fun `get history query`() = runTestOnMain {
        assertEquals(0, history.queryHistoryMetadata("keystore", 1).size)

        val metaKey1 = HistoryMetadataKey(
            url = "https://sql.telemetry.mozilla.org/dashboard/android-keystore-reliability-experiment",
            searchTerm = "keystore reliability",
            referrerUrl = "http://self.mozilla.com/",
        )
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular))
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.ViewTimeObservation(20000))

        val metaKey2 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=F7PQdCDiE44",
            searchTerm = "crisis",
            referrerUrl = "https://www.google.com/search?client=firefox-b-d&q=dw+crisis",
        )
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.ViewTimeObservation(30000))

        history.writer.noteObservation(
            VisitObservation(
                url = "https://www.youtube.com/watch?v=F7PQdCDiE44",
                title = "DW next crisis",
                visitType = mozilla.appservices.places.uniffi.VisitTransition.LINK,
            ),
        )

        val metaKey3 = HistoryMetadataKey(
            url = "https://www.cbc.ca/news/canada/toronto/covid-19-ontario-april-16-2021-new-restrictions-modelling-1.5990092",
            searchTerm = "ford covid19",
            referrerUrl = "https://duckduckgo.com/?q=ford+covid19&t=hc&va=u&ia=web",
        )
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular))
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.ViewTimeObservation(20000))

        val metaKey4 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=TfXbzbJQHuw",
            searchTerm = "dw nyc rich",
            referrerUrl = "https://duckduckgo.com/?q=dw+nyc+rich&t=hc&va=u&ia=web",
        )
        history.noteHistoryMetadataObservation(metaKey4, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey4, HistoryMetadataObservation.ViewTimeObservation(20000))

        assertEquals(0, history.queryHistoryMetadata("keystore", 0).size)

        // query by url
        with(history.queryHistoryMetadata("dashboard", 10)) {
            assertEquals(1, this.size)
            assertHistoryMetadataRecord(metaKey1, 20000, DocumentType.Regular, this[0])
        }

        // query by title
        with(history.queryHistoryMetadata("next crisis", 10)) {
            assertEquals(1, this.size)
            assertHistoryMetadataRecord(metaKey2, 30000, DocumentType.Media, this[0])
        }

        // query by search term
        with(history.queryHistoryMetadata("covid19", 10)) {
            assertEquals(1, this.size)
            assertHistoryMetadataRecord(metaKey3, 20000, DocumentType.Regular, this[0])
        }

        // multiple results, mixed search targets
        with(history.queryHistoryMetadata("dw", 10)) {
            assertEquals(2, this.size)
            assertHistoryMetadataRecord(metaKey2, 30000, DocumentType.Media, this[0])
            assertHistoryMetadataRecord(metaKey4, 20000, DocumentType.Media, this[1])
        }

        // limit is respected
        with(history.queryHistoryMetadata("dw", 1)) {
            assertEquals(1, this.size)
            assertHistoryMetadataRecord(metaKey2, 30000, DocumentType.Media, this[0])
        }
    }

    @Test
    fun `get history metadata between`() = runTestOnMain {
        assertEquals(0, history.getHistoryMetadataBetween(-1, 0).size)
        assertEquals(0, history.getHistoryMetadataBetween(0, Long.MAX_VALUE).size)
        assertEquals(0, history.getHistoryMetadataBetween(Long.MAX_VALUE, Long.MIN_VALUE).size)
        assertEquals(0, history.getHistoryMetadataBetween(Long.MIN_VALUE, Long.MAX_VALUE).size)

        val beginning = System.currentTimeMillis()

        val metaKey1 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=lNeRQuiKBd4",
            referrerUrl = "http://www.twitter.com",
        )
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.ViewTimeObservation(20000))
        val afterMeta1 = System.currentTimeMillis()

        val metaKey2 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=Cs1b5qvCZ54",
            searchTerm = "путин валдай",
            referrerUrl = "http://www.yandex.ru",
        )
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.ViewTimeObservation(200))
        val afterMeta2 = System.currentTimeMillis()

        val metaKey3 = HistoryMetadataKey(
            url = "https://www.ifixit.com/News/35377/which-wireless-earbuds-are-the-least-evil",
            searchTerm = "repairable wireless headset",
            referrerUrl = "http://www.google.com",
        )
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular))
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.ViewTimeObservation(2000))

        assertEquals(3, history.getHistoryMetadataBetween(0, Long.MAX_VALUE).size)
        assertEquals(0, history.getHistoryMetadataBetween(Long.MAX_VALUE, 0).size)
        assertEquals(0, history.getHistoryMetadataBetween(Long.MIN_VALUE, 0).size)

        with(history.getHistoryMetadataBetween(beginning, afterMeta1)) {
            assertEquals(1, this.size)
            assertEquals("https://www.youtube.com/watch?v=lNeRQuiKBd4", this[0].key.url)
        }
        with(history.getHistoryMetadataBetween(beginning, afterMeta2)) {
            assertEquals(2, this.size)
            assertEquals("https://www.youtube.com/watch?v=Cs1b5qvCZ54", this[0].key.url)
            assertEquals("https://www.youtube.com/watch?v=lNeRQuiKBd4", this[1].key.url)
        }
        with(history.getHistoryMetadataBetween(afterMeta1, afterMeta2)) {
            assertEquals(1, this.size)
            assertEquals("https://www.youtube.com/watch?v=Cs1b5qvCZ54", this[0].key.url)
        }
    }

    @Test
    fun `get history metadata since`() = runTestOnMain {
        val beginning = System.currentTimeMillis()

        assertEquals(0, history.getHistoryMetadataSince(-1).size)
        assertEquals(0, history.getHistoryMetadataSince(0).size)
        assertEquals(0, history.getHistoryMetadataSince(Long.MIN_VALUE).size)
        assertEquals(0, history.getHistoryMetadataSince(Long.MAX_VALUE).size)
        assertEquals(0, history.getHistoryMetadataSince(beginning).size)

        val metaKey1 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=lNeRQuiKBd4",
            referrerUrl = "http://www.twitter.com/",
        )
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey1, HistoryMetadataObservation.ViewTimeObservation(20000))

        val afterMeta1 = System.currentTimeMillis()

        val metaKey2 = HistoryMetadataKey(
            url = "https://www.ifixit.com/News/35377/which-wireless-earbuds-are-the-least-evil",
            searchTerm = "repairable wireless headset",
            referrerUrl = "http://www.google.com/",
        )
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular))
        history.noteHistoryMetadataObservation(metaKey2, HistoryMetadataObservation.ViewTimeObservation(2000))

        val afterMeta2 = System.currentTimeMillis()

        val metaKey3 = HistoryMetadataKey(
            url = "https://www.youtube.com/watch?v=Cs1b5qvCZ54",
            searchTerm = "путин валдай",
            referrerUrl = "http://www.yandex.ru/",
        )
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media))
        history.noteHistoryMetadataObservation(metaKey3, HistoryMetadataObservation.ViewTimeObservation(200))

        // order is by updatedAt
        with(history.getHistoryMetadataSince(beginning)) {
            assertEquals(3, this.size)
            assertHistoryMetadataRecord(metaKey3, 200, DocumentType.Media, this[0])
            assertHistoryMetadataRecord(metaKey2, 2000, DocumentType.Regular, this[1])
            assertHistoryMetadataRecord(metaKey1, 20000, DocumentType.Media, this[2])
        }

        // search is inclusive of time
        with(history.getHistoryMetadataSince(afterMeta1)) {
            assertEquals(2, this.size)
            assertHistoryMetadataRecord(metaKey3, 200, DocumentType.Media, this[0])
            assertHistoryMetadataRecord(metaKey2, 2000, DocumentType.Regular, this[1])
        }

        with(history.getHistoryMetadataSince(afterMeta2)) {
            assertEquals(1, this.size)
            assertHistoryMetadataRecord(metaKey3, 200, DocumentType.Media, this[0])
        }
    }

    @Test
    fun `delete history metadata by search term`() = runTestOnMain {
        // Able to operate against an empty db
        history.deleteHistoryMetadata("test")
        history.deleteHistoryMetadata("")

        // Observe some items.
        with(
            HistoryMetadataKey(
                url = "https://www.youtube.com/watch?v=lNeRQuiKBd4",
                referrerUrl = "http://www.twitter.com/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(20000),
            )
        }

        // For the ifixit case, let's create a scenario with metadata entries that will dedupe by url,
        // and will have at least some 0 view time records. In a search term group, these 4 metadata
        // records will show up as 2, so a naive delete (iterate through group, delete) will leave records
        // behind.
        with(
            HistoryMetadataKey(
                url = "https://www.ifixit.com/News/35377/which-wireless-earbuds-are-the-least-evil",
                searchTerm = "repairable wireless headset",
                referrerUrl = "http://www.google.com/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(2000),
            )
        }
        // Same search term as above, different url/referrer.
        with(
            HistoryMetadataKey(
                url = "https://www.youtube.com/watch?v=rfdshufsSfsd",
                searchTerm = "repairable wireless headset",
                referrerUrl = null,
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(25000),
            )
        }
        // Same search term as above, same url, different referrer.
        with(
            HistoryMetadataKey(
                url = "https://www.ifixit.com/News/35377/which-wireless-earbuds-are-the-least-evil",
                searchTerm = "repairable wireless headset",
                referrerUrl = "http://www.yandex.ru/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(1000),
            )
        }
        // Again, but without view time.
        with(
            HistoryMetadataKey(
                url = "https://www.ifixit.com/News/35377/which-wireless-earbuds-are-the-least-evil",
                searchTerm = "repairable wireless headset",
                referrerUrl = null,
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
        }

        // No view time for this one.
        with(
            HistoryMetadataKey(
                url = "https://www.youtube.com/watch?v=Cs1b5qvCZ54",
                searchTerm = "путин валдай",
                referrerUrl = "http://www.yandex.ru/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Media),
            )
        }

        assertEquals(6, history.getHistoryMetadataSince(0).count())

        history.deleteHistoryMetadata("repairable wireless headset")
        assertEquals(2, history.getHistoryMetadataSince(0).count())

        history.deleteHistoryMetadata("Путин Валдай")
        assertEquals(1, history.getHistoryMetadataSince(0).count())
    }

    @Test
    fun `safe read from places`() = runTestOnMain {
        val result = history.handlePlacesExceptions("test", default = emptyList<HistoryMetadata>()) {
            // Can be any PlacesException error
            throw PlacesException.PlacesConnectionBusy("test")
        }
        assertEquals(emptyList<HistoryMetadata>(), result)
    }

    @Test
    fun `interrupted read from places is not reported to crash services and returns the default`() = runTestOnMain {
        val result = history.handlePlacesExceptions("test", default = emptyList<HistoryMetadata>()) {
            throw PlacesException.OperationInterrupted("An interrupted in progress query will throw")
        }

        verify(history.crashReporter!!, never()).submitCaughtException(any())
        assertEquals(emptyList<HistoryMetadata>(), result)
    }

    @Test
    fun `history delegate's shouldStoreUri works as expected`() {
        // Not an excessive list of allowed schemes.
        assertTrue(history.canAddUri("http://www.mozilla.com"))
        assertTrue(history.canAddUri("https://www.mozilla.com"))
        assertTrue(history.canAddUri("ftp://files.mozilla.com/stuff/fenix.apk"))
        assertTrue(history.canAddUri("about:reader?url=http://www.mozilla.com/interesting-article.html"))
        assertTrue(history.canAddUri("https://john.doe@www.example.com:123/forum/questions/?tag=networking&order=newest#top"))
        assertTrue(history.canAddUri("ldap://2001:db8::7/c=GB?objectClass?one"))
        assertTrue(history.canAddUri("telnet://192.0.2.16:80/"))

        assertFalse(history.canAddUri("withoutSchema.html"))
        assertFalse(history.canAddUri("about:blank"))
        assertFalse(history.canAddUri("news:comp.infosystems.www.servers.unix"))
        assertFalse(history.canAddUri("imap://mail.example.com/~mozilla"))
        assertFalse(history.canAddUri("chrome://config"))
        assertFalse(history.canAddUri("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D"))
        assertFalse(history.canAddUri("data:text/html,<script>alert('hi');</script>"))
        assertFalse(history.canAddUri("resource://internal-thingy-js-inspector/script.js"))
        assertFalse(history.canAddUri("javascript:alert('hello!');"))
        assertFalse(history.canAddUri("blob:https://api.mozilla.com/resource.png"))
    }

    @Test
    fun `delete history metadata by url`() = runTestOnMain {
        // Able to operate against an empty db
        history.deleteHistoryMetadataForUrl("https://mozilla.org")
        history.deleteHistoryMetadataForUrl("")

        // Observe some items.
        with(
            HistoryMetadataKey(
                url = "https://firefox.com/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(20000),
            )
        }

        with(
            HistoryMetadataKey(
                url = "https://mozilla.org/",
                searchTerm = "firefox",
                referrerUrl = "https://google.com/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(20000),
            )
        }

        with(
            HistoryMetadataKey(
                url = "https://getpocket.com/",
            ),
        ) {
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.DocumentTypeObservation(DocumentType.Regular),
            )
            history.noteHistoryMetadataObservation(
                this,
                HistoryMetadataObservation.ViewTimeObservation(20000),
            )
        }

        assertEquals(3, history.getHistoryMetadataSince(0).count())

        history.deleteHistoryMetadataForUrl("https://firefox.com/")
        assertEquals(2, history.getHistoryMetadataSince(0).count())
        assertEquals("https://getpocket.com/", history.getHistoryMetadataSince(0)[0].key.url)
        assertEquals("https://mozilla.org/", history.getHistoryMetadataSince(0)[1].key.url)

        history.deleteHistoryMetadataForUrl("https://mozilla.org/")
        assertEquals(1, history.getHistoryMetadataSince(0).count())
        assertEquals("https://getpocket.com/", history.getHistoryMetadataSince(0)[0].key.url)

        history.deleteHistoryMetadataForUrl("https://getpocket.com/")
        assertEquals(0, history.getHistoryMetadataSince(0).count())
    }

    private fun assertHistoryMetadataRecord(
        expectedKey: HistoryMetadataKey,
        expectedTotalViewTime: Int,
        expectedDocumentType: DocumentType,
        db_meta: HistoryMetadata,
    ) {
        assertEquals(expectedKey, db_meta.key)
        assertEquals(expectedTotalViewTime, db_meta.totalViewTime)
        assertEquals(expectedDocumentType, db_meta.documentType)
    }
}

fun getTestPath(path: String): File {
    return PlacesHistoryStorage::class.java.classLoader!!
        .getResource(path).file
        .let { File(it) }
}
