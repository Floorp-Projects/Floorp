/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.InternalPanic
import mozilla.appservices.places.PlacesException
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class PlacesHistoryStorageTest {
    private lateinit var history: PlacesHistoryStorage

    @Before
    fun setup() = runBlocking {
        history = PlacesHistoryStorage(testContext)
        // There's a database on disk which needs to be cleaned up between tests.
        history.deleteEverything()
    }

    @After
    fun cleanup() = runBlocking {
        history.cleanup()
    }

    @Test
    fun `storage allows recording and querying visits of different types`() = runBlocking {
        history.recordVisit("http://www.firefox.com/1", VisitType.LINK)
        history.recordVisit("http://www.firefox.com/2", VisitType.RELOAD)
        history.recordVisit("http://www.firefox.com/3", VisitType.TYPED)
        history.recordVisit("http://www.firefox.com/4", VisitType.REDIRECT_TEMPORARY)
        history.recordVisit("http://www.firefox.com/5", VisitType.REDIRECT_PERMANENT)
        history.recordVisit("http://www.firefox.com/6", VisitType.FRAMED_LINK)
        history.recordVisit("http://www.firefox.com/7", VisitType.EMBED)
        history.recordVisit("http://www.firefox.com/8", VisitType.BOOKMARK)
        history.recordVisit("http://www.firefox.com/9", VisitType.DOWNLOAD)

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
        assertEquals(listOf(
                "http://www.firefox.com/1", "http://www.firefox.com/2", "http://www.firefox.com/3",
                "http://www.firefox.com/4", "http://www.firefox.com/5", "http://www.firefox.com/6",
                "http://www.firefox.com/7", "http://www.firefox.com/8", "http://www.firefox.com/9"
        ), history.getVisited())

        // Can use GeckoView-style getVisited API.
        assertEquals(
                listOf(false, true, true, true, true, true, true, false, true, true, true),
                history.getVisited(listOf(
                        "http://www.mozilla.com",
                        "http://www.firefox.com/1", "http://www.firefox.com/2", "http://www.firefox.com/3",
                        "http://www.firefox.com/4", "http://www.firefox.com/5", "http://www.firefox.com/6",
                        "http://www.firefox.com/oops",
                        "http://www.firefox.com/7", "http://www.firefox.com/8", "http://www.firefox.com/9"
                ))
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
    fun `storage passes through recordObservation calls`() = runBlocking {
        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordObservation("http://www.mozilla.org", PageObservation(title = "Mozilla"))

        val recordedVisits = history.getDetailedVisits(0)
        assertEquals(1, recordedVisits.size)
        assertEquals("Mozilla", recordedVisits[0].title)
    }

    @Test
    fun `store can be used to query detailed visit information`() = runBlocking {
        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        history.recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
        history.recordVisit("http://www.firefox.com", VisitType.LINK)

        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_TEMPORARY)

        val visits = history.getDetailedVisits(0, excludeTypes = listOf(VisitType.REDIRECT_TEMPORARY))
        assertEquals(3, visits.size)
        assertEquals("http://www.mozilla.org/", visits[0].url)
        assertEquals("Mozilla", visits[0].title)
        assertEquals(VisitType.LINK, visits[0].visitType)

        assertEquals("http://www.mozilla.org/", visits[1].url)
        assertEquals("Mozilla", visits[1].title)
        assertEquals(VisitType.RELOAD, visits[1].visitType)

        assertEquals("http://www.firefox.com/", visits[2].url)
        assertEquals("", visits[2].title)
        assertEquals(VisitType.LINK, visits[2].visitType)

        val visitsAll = history.getDetailedVisits(0)
        assertEquals(4, visitsAll.size)
    }

    @Test
    fun `store can be used to record and retrieve history via webview-style callbacks`() = runBlocking {
        // Empty.
        assertEquals(0, history.getVisited().size)

        // Regular visits are tracked.
        history.recordVisit("https://www.mozilla.org", VisitType.LINK)
        assertEquals(listOf("https://www.mozilla.org/"), history.getVisited())

        // Multiple visits can be tracked, results ordered by "URL's first seen first".
        history.recordVisit("https://www.firefox.com", VisitType.LINK)
        assertEquals(listOf("https://www.mozilla.org/", "https://www.firefox.com/"), history.getVisited())

        // Visits marked as reloads can be tracked.
        history.recordVisit("https://www.firefox.com", VisitType.RELOAD)
        assertEquals(listOf("https://www.mozilla.org/", "https://www.firefox.com/"), history.getVisited())

        // Visited urls are certainly a set.
        history.recordVisit("https://www.firefox.com", VisitType.LINK)
        history.recordVisit("https://www.mozilla.org", VisitType.LINK)
        history.recordVisit("https://www.wikipedia.org", VisitType.LINK)
        assertEquals(
                listOf("https://www.mozilla.org/", "https://www.firefox.com/", "https://www.wikipedia.org/"),
                history.getVisited()
        )
    }

    @Test
    fun `store can be used to record and retrieve history via gecko-style callbacks`() = runBlocking {
        assertEquals(0, history.getVisited(listOf()).size)

        // Regular visits are tracked
        history.recordVisit("https://www.mozilla.org", VisitType.LINK)
        assertEquals(listOf(true), history.getVisited(listOf("https://www.mozilla.org")))

        // Duplicate requests are handled.
        assertEquals(listOf(true, true), history.getVisited(listOf("https://www.mozilla.org", "https://www.mozilla.org")))

        // Visit map is returned in correct order.
        assertEquals(listOf(true, false), history.getVisited(listOf("https://www.mozilla.org", "https://www.unknown.com")))

        assertEquals(listOf(false, true), history.getVisited(listOf("https://www.unknown.com", "https://www.mozilla.org")))

        // Multiple visits can be tracked. Reloads can be tracked.
        history.recordVisit("https://www.firefox.com", VisitType.LINK)
        history.recordVisit("https://www.mozilla.org", VisitType.RELOAD)
        history.recordVisit("https://www.wikipedia.org", VisitType.LINK)
        assertEquals(listOf(true, true, false, true), history.getVisited(listOf("https://www.firefox.com", "https://www.wikipedia.org", "https://www.unknown.com", "https://www.mozilla.org")))
    }

    @Test
    fun `store can be used to track page meta information - title changes`() = runBlocking {
        // Title changes are recorded.
        history.recordVisit("https://www.wikipedia.org", VisitType.TYPED)
        history.recordObservation("https://www.wikipedia.org", PageObservation("Wikipedia"))
        var recorded = history.getDetailedVisits(0)
        assertEquals(1, recorded.size)
        assertEquals("Wikipedia", recorded[0].title)

        history.recordObservation("https://www.wikipedia.org", PageObservation("Википедия"))
        recorded = history.getDetailedVisits(0)
        assertEquals(1, recorded.size)
        assertEquals("Википедия", recorded[0].title)

        // Titles for different pages are recorded.
        history.recordVisit("https://www.firefox.com", VisitType.TYPED)
        history.recordObservation("https://www.firefox.com", PageObservation("Firefox"))
        history.recordVisit("https://www.mozilla.org", VisitType.TYPED)
        history.recordObservation("https://www.mozilla.org", PageObservation("Мозилла"))
        recorded = history.getDetailedVisits(0)
        assertEquals(3, recorded.size)
        assertEquals("Википедия", recorded[0].title)
        assertEquals("Firefox", recorded[1].title)
        assertEquals("Мозилла", recorded[2].title)
    }

    @Test
    fun `store can provide suggestions`() = runBlocking {
        assertEquals(0, history.getSuggestions("Mozilla", 100).size)

        history.recordVisit("http://www.firefox.com", VisitType.LINK)
        val search = history.getSuggestions("Mozilla", 100)
        assertEquals(0, search.size)

        history.recordVisit("http://www.wikipedia.org", VisitType.LINK)
        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordVisit("http://www.moscow.ru", VisitType.LINK)
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
    fun `store can provide autocomplete suggestions`() = runBlocking {
        assertNull(history.getAutocompleteSuggestion("moz"))

        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        var res = history.getAutocompleteSuggestion("moz")!!
        assertEquals("mozilla.org/", res.text)
        assertEquals("http://www.mozilla.org/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        history.recordVisit("http://firefox.com", VisitType.LINK)
        res = history.getAutocompleteSuggestion("firefox")!!
        assertEquals("firefox.com/", res.text)
        assertEquals("http://firefox.com/", res.url)
        assertEquals("placesHistory", res.source)
        assertEquals(1, res.totalItems)

        history.recordVisit("https://en.wikipedia.org/wiki/Mozilla", VisitType.LINK)
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

        assertNull(history.getAutocompleteSuggestion("hello"))
    }

    @Test
    fun `store ignores url parse exceptions during record operations`() = runBlocking {
        // These aren't valid URIs, and if we're not explicitly ignoring exceptions from the underlying
        // storage layer, these calls will throw.
        history.recordVisit("mozilla.org", VisitType.LINK)
        history.recordObservation("mozilla.org", PageObservation("mozilla"))
    }

    @Test
    fun `store can delete everything`() = runBlocking {
        history.recordVisit("http://www.mozilla.org", VisitType.TYPED)
        history.recordVisit("http://www.mozilla.org", VisitType.DOWNLOAD)
        history.recordVisit("http://www.mozilla.org", VisitType.BOOKMARK)
        history.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        history.recordVisit("http://www.firefox.com", VisitType.EMBED)
        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_PERMANENT)
        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_TEMPORARY)
        history.recordVisit("http://www.firefox.com", VisitType.LINK)

        history.recordObservation("http://www.firefox.com", PageObservation("Firefox"))

        assertEquals(2, history.getVisited().size)

        history.deleteEverything()

        assertEquals(0, history.getVisited().size)
    }

    @Test
    fun `store can delete by url`() = runBlocking {
        history.recordVisit("http://www.mozilla.org", VisitType.TYPED)
        history.recordVisit("http://www.mozilla.org", VisitType.DOWNLOAD)
        history.recordVisit("http://www.mozilla.org", VisitType.BOOKMARK)
        history.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        history.recordVisit("http://www.firefox.com", VisitType.EMBED)
        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_PERMANENT)
        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_TEMPORARY)
        history.recordVisit("http://www.firefox.com", VisitType.LINK)

        history.recordObservation("http://www.firefox.com", PageObservation("Firefox"))

        assertEquals(2, history.getVisited().size)

        history.deleteVisitsFor("http://www.mozilla.org")

        assertEquals(1, history.getVisited().size)
        assertEquals("http://www.firefox.com/", history.getVisited()[0])

        history.deleteVisitsFor("http://www.firefox.com")
        assertEquals(0, history.getVisited().size)
    }

    @Test
    fun `store can delete by 'since'`() = runBlocking {
        history.recordVisit("http://www.mozilla.org", VisitType.TYPED)
        history.recordVisit("http://www.mozilla.org", VisitType.DOWNLOAD)
        history.recordVisit("http://www.mozilla.org", VisitType.BOOKMARK)

        history.deleteVisitsSince(0)
        val visits = history.getVisited()
        assertEquals(0, visits.size)
    }

    @Test
    fun `store can delete by 'range'`() {
        runBlocking {
            history.recordVisit("http://www.mozilla.org/1", VisitType.TYPED)
            Thread.sleep(10)
            history.recordVisit("http://www.mozilla.org/2", VisitType.DOWNLOAD)
            Thread.sleep(10)
            history.recordVisit("http://www.mozilla.org/3", VisitType.BOOKMARK)
        }

        val ts = runBlocking {
            val visits = history.getDetailedVisits(0, Long.MAX_VALUE)

            assertEquals(3, visits.size)
            visits[1].visitTime
        }

        runBlocking {
            history.deleteVisitsBetween(ts - 1, ts + 1)
        }
        val visits = runBlocking {
            history.getDetailedVisits(0, Long.MAX_VALUE)
        }
        assertEquals(2, visits.size)

        assertEquals("http://www.mozilla.org/1", visits[0].url)
        assertEquals("http://www.mozilla.org/3", visits[1].url)
    }

    @Test
    fun `store can delete visit by 'url' and 'timestamp'`() {
        runBlocking {
            history.recordVisit("http://www.mozilla.org/1", VisitType.TYPED)
            Thread.sleep(10)
            history.recordVisit("http://www.mozilla.org/2", VisitType.DOWNLOAD)
            Thread.sleep(10)
            history.recordVisit("http://www.mozilla.org/3", VisitType.BOOKMARK)
        }

        val ts = runBlocking {
            val visits = history.getDetailedVisits(0, Long.MAX_VALUE)

            assertEquals(3, visits.size)
            visits[1].visitTime
        }

        runBlocking {
            history.deleteVisit("http://www.mozilla.org/4", 111)
            // There are no visits for this url, delete is a no-op.
            assertEquals(3, history.getDetailedVisits(0, Long.MAX_VALUE).size)
        }

        runBlocking {
            history.deleteVisit("http://www.mozilla.org/1", ts)
            // There is no such visit for this url, delete is a no-op.
            assertEquals(3, history.getDetailedVisits(0, Long.MAX_VALUE).size)
        }

        runBlocking {
            history.deleteVisit("http://www.mozilla.org/2", ts)
        }

        val visits = runBlocking {
            history.getDetailedVisits(0, Long.MAX_VALUE)
        }
        assertEquals(2, visits.size)

        assertEquals("http://www.mozilla.org/1", visits[0].url)
        assertEquals("http://www.mozilla.org/3", visits[1].url)
    }

    @Test
    fun `can run maintanence on the store`() = runBlocking {
        history.runMaintenance()
    }

    @Test
    fun `can run prune on the store`() = runBlocking {
        // Empty.
        history.prune()
        history.recordVisit("http://www.mozilla.org/1", VisitType.TYPED)
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
    fun `storage passes through sync calls`() = runBlocking {
        var passedAuthInfo: SyncAuthInfo? = null
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
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

            override fun getHandle(): Long {
                fail()
                return 0L
            }

            override fun close() {
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
    fun `storage passes through sync OK results`() = runBlocking {
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
                fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {}

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {}

            override fun getHandle(): Long {
                fail()
                return 0L
            }

            override fun close() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))
        assertEquals(SyncStatus.Ok, result)
    }

    @Test
    fun `storage passes through sync exceptions`() = runBlocking {
        val exception = PlacesException("test error")
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
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

            override fun getHandle(): Long {
                fail()
                return 0L
            }

            override fun close() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        assertTrue(result is SyncStatus.Error)

        val error = result as SyncStatus.Error
        assertEquals("test error", error.exception.message)
    }

    @Test(expected = InternalPanic::class)
    fun `storage re-throws sync panics`() = runBlocking {
        val exception = InternalPanic("test panic")
        val conn = object : Connection {
            override fun reader(): PlacesReaderConnection {
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

            override fun getHandle(): Long {
                fail()
                return 0L
            }

            override fun close() {
                fail()
            }
        }
        val storage = MockingPlacesHistoryStorage(conn)
        storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))
        fail()
    }
}
