/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.PlacesException
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.appservices.places.SearchResult
import mozilla.appservices.places.VisitInfo
import mozilla.appservices.places.VisitObservation
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.AuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class PlacesHistoryStorageTest {
    private var conn: Connection? = null
    private var reader: PlacesReaderConnection? = null
    private var writer: PlacesWriterConnection? = null

    private var storage: PlacesHistoryStorage? = null

    class TestablePlacesHistoryStorage(override val places: Connection) : PlacesHistoryStorage(RuntimeEnvironment.application)

    @Before
    fun setup() {
        conn = mock()
        reader = mock()
        writer = mock()
        `when`(conn!!.reader()).thenReturn(reader)
        `when`(conn!!.writer()).thenReturn(writer)
        storage = TestablePlacesHistoryStorage(conn!!)
    }

    @Test
    fun `storage passes through recordVisit calls`() = runBlocking {
        val storage = storage!!
        val writer = writer!!

        storage.recordVisit("http://www.mozilla.org", VisitType.LINK)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = mozilla.appservices.places.VisitType.LINK)
        )

        storage.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = mozilla.appservices.places.VisitType.RELOAD)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.TYPED)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.TYPED)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.REDIRECT_TEMPORARY)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.REDIRECT_PERMANENT)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.REDIRECT_PERMANENT)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.FRAMED_LINK)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.FRAMED_LINK)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.EMBED)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.EMBED)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.BOOKMARK)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.BOOKMARK)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.DOWNLOAD)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.DOWNLOAD)
        )
    }

    @Test
    fun `storage passes through recordObservation calls`() = runBlocking {
        val storage = storage!!
        val writer = writer!!

        storage.recordObservation("http://www.mozilla.org", PageObservation(title = "Mozilla"))
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = mozilla.appservices.places.VisitType.UPDATE_PLACE, title = "Mozilla")
        )

        storage.recordObservation("http://www.firefox.com", PageObservation(title = null))
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = mozilla.appservices.places.VisitType.UPDATE_PLACE, title = null)
        )
    }

    @Test
    fun `storage passes through getVisited(uris) calls`() = runBlocking {
        val storage = storage!!
        val reader = reader!!

        storage.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))
        verify(reader, times(1)).getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))

        storage.getVisited(listOf())
        verify(reader, times(1)).getVisited(listOf())

        Unit
    }

    @Test
    fun `storage passes through getVisited() calls`() = runBlocking {
        val storage = storage!!
        val reader = reader!!

        storage.getVisited()
        verify(reader, times(1)).getVisitedUrlsInRange(eq(0), ArgumentMatchers.anyLong(), eq(true))
        Unit
    }

    @Test
    fun `storage passes through getDetailedVisits() calls`() = runBlocking {
        val storage = storage!!
        val reader = reader!!

        storage.getDetailedVisits(15, 25)
        verify(reader, times(1)).getVisitInfos(eq(15), eq(25))

        storage.getDetailedVisits(12345)
        verify(reader, times(1)).getVisitInfos(eq(12345), eq(Long.MAX_VALUE))

        `when`(reader.getVisitInfos(15, 25)).thenReturn(listOf(
            VisitInfo(
                url = "http://www.mozilla.org",
                visitType = mozilla.appservices.places.VisitType.TYPED,
                visitTime = 17,
                title = null
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.BOOKMARK,
                visitTime = 20,
                title = "Firefox"
            ),
            // All other visit types, so that we can check that visit types are being converted.
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.RELOAD,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.LINK,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.DOWNLOAD,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.EMBED,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.FRAMED_LINK,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.REDIRECT_PERMANENT,
                visitTime = 20,
                title = "Firefox"
            ),
            VisitInfo(
                url = "http://www.firefox.com",
                visitType = mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY,
                visitTime = 20,
                title = "Firefox"
            )
        ))
        val visits = storage.getDetailedVisits(15, 25)
        assertEquals(9, visits.size)
        // Assert type conversions.
        assertEquals("http://www.mozilla.org", visits[0].url)
        assertEquals(VisitType.TYPED, visits[0].visitType)
        assertEquals(17, visits[0].visitTime)
        assertEquals(null, visits[0].title)

        assertEquals("http://www.firefox.com", visits[1].url)
        assertEquals(VisitType.BOOKMARK, visits[1].visitType)
        assertEquals(20, visits[1].visitTime)
        assertEquals("Firefox", visits[1].title)

        // Visit type assertions.
        assertEquals(VisitType.RELOAD, visits[2].visitType)
        assertEquals(VisitType.LINK, visits[3].visitType)
        assertEquals(VisitType.DOWNLOAD, visits[4].visitType)
        assertEquals(VisitType.EMBED, visits[5].visitType)
        assertEquals(VisitType.FRAMED_LINK, visits[6].visitType)
        assertEquals(VisitType.REDIRECT_PERMANENT, visits[7].visitType)
        assertEquals(VisitType.REDIRECT_TEMPORARY, visits[8].visitType)
    }

    @Test
    fun `storage passes through getSuggestions calls`() {
        val storage = storage!!
        val reader = reader!!

        storage.getSuggestions("Hello!", 10)
        verify(reader, times(1)).queryAutocomplete("Hello!", 10)

        storage.getSuggestions("World!", 0)
        verify(reader, times(1)).queryAutocomplete("World!", 0)

        `when`(reader.queryAutocomplete("mozilla", 10)).thenReturn(listOf(
                SearchResult("mozilla", "http://www.mozilla.org", "Mozilla", 10),
                SearchResult("mozilla", "http://www.firefox.com", "Mozilla Firefox", 5),
                SearchResult("mozilla", "https://en.wikipedia.org/wiki/Mozilla", "", 8))
        )
        val results = storage.getSuggestions("mozilla", 10)
        assertEquals(3, results.size)
        assertEquals(
            mozilla.components.concept.storage.SearchResult(
                "http://www.mozilla.org",
                "http://www.mozilla.org",
                10,
                "Mozilla"
            ), results[0]
        )
        assertEquals(
            mozilla.components.concept.storage.SearchResult(
                "http://www.firefox.com",
                "http://www.firefox.com",
                5,
                "Mozilla Firefox"
            ), results[1]
        )
        assertEquals(
            mozilla.components.concept.storage.SearchResult(
                "https://en.wikipedia.org/wiki/Mozilla",
                "https://en.wikipedia.org/wiki/Mozilla",
                8,
                ""
            ), results[2]
        )
    }

    @Test(expected = IllegalArgumentException::class)
    fun `storage validates calls to getSuggestion`() {
        storage!!.getSuggestions("Hello!", -1)
    }

    @Test
    fun `storage passes through getAutocompleteSuggestion calls`() {
        val storage = storage!!
        val reader = reader!!
        `when`(reader.matchUrl("mozilla")).thenReturn("http://www.mozilla.org")
        val res = storage.getAutocompleteSuggestion("mozilla")!!
        assertEquals(1, res.totalItems)
        assertEquals("http://www.mozilla.org", res.url)
        assertEquals("mozilla.org", res.text)
        assertEquals("placesHistory", res.source)

        assertNull(storage.getAutocompleteSuggestion("hello"))
    }

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

            override fun sync(syncInfo: SyncAuthInfo) {
                assertNull(passedAuthInfo)
                passedAuthInfo = syncInfo
            }

            override fun close() {
                fail()
            }
        }
        val storage = TestablePlacesHistoryStorage(conn)

        storage.sync(AuthInfo("kid", "token", "key", "serverUrl"))

        assertEquals("kid", passedAuthInfo!!.kid)
        assertEquals("serverUrl", passedAuthInfo!!.tokenserverURL)
        assertEquals("token", passedAuthInfo!!.fxaAccessToken)
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

            override fun sync(syncInfo: SyncAuthInfo) {}

            override fun close() {
                fail()
            }
        }
        val storage = TestablePlacesHistoryStorage(conn)

        val result = storage.sync(AuthInfo("kid", "token", "key", "serverUrl"))
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

            override fun sync(syncInfo: SyncAuthInfo) {
                throw exception
            }

            override fun close() {
                fail()
            }
        }
        val storage = TestablePlacesHistoryStorage(conn)

        val result = storage.sync(AuthInfo("kid", "token", "key", "serverUrl"))
        assertEquals(SyncStatus.Error(exception), result)
    }

    @Test
    fun `storage passes through calls to deleteEverything`() = runBlocking {
        val storage = storage!!
        val writer = writer!!
        storage.deleteEverything()
        verify(writer).deleteEverything()
        verify(writer, never()).deleteVisitsBetween(ArgumentMatchers.anyLong(), ArgumentMatchers.anyLong())
        verify(writer, never()).deleteVisitsSince(ArgumentMatchers.anyLong())
        verify(writer, never()).deletePlace(ArgumentMatchers.anyString())
        verify(writer, never()).deleteVisit(ArgumentMatchers.anyString(), ArgumentMatchers.anyLong())
        Unit
    }

    @Test
    fun `storage passes through calls to deleteVisitsBetween`() = runBlocking {
        val storage = storage!!
        val writer = writer!!
        storage.deleteVisitsBetween(15, 20)
        verify(writer, never()).deleteEverything()
        verify(writer).deleteVisitsBetween(15, 20)
        verify(writer, never()).deleteVisitsSince(ArgumentMatchers.anyLong())
        verify(writer, never()).deletePlace(ArgumentMatchers.anyString())
        verify(writer, never()).deleteVisit(ArgumentMatchers.anyString(), ArgumentMatchers.anyLong())
        Unit
    }

    @Test
    fun `storage passes through calls to deleteVisitsSince`() = runBlocking {
        val storage = storage!!
        val writer = writer!!
        storage.deleteVisitsSince(15)
        verify(writer, never()).deleteEverything()
        verify(writer, never()).deleteVisitsBetween(ArgumentMatchers.anyLong(), ArgumentMatchers.anyLong())
        verify(writer).deleteVisitsSince(15)
        verify(writer, never()).deletePlace(ArgumentMatchers.anyString())
        verify(writer, never()).deleteVisit(ArgumentMatchers.anyString(), ArgumentMatchers.anyLong())
        Unit
    }

    @Test
    fun `storage passes through calls to deleteVisitsFor`() = runBlocking {
        val storage = storage!!
        val writer = writer!!
        storage.deleteVisitsFor("http://www.firefox.com")
        verify(writer, never()).deleteEverything()
        verify(writer, never()).deleteVisitsBetween(ArgumentMatchers.anyLong(), ArgumentMatchers.anyLong())
        verify(writer, never()).deleteVisitsSince(ArgumentMatchers.anyLong())
        verify(writer).deletePlace("http://www.firefox.com")
        verify(writer, never()).deleteVisit(ArgumentMatchers.anyString(), ArgumentMatchers.anyLong())
        Unit
    }

    @Test
    fun `storage passes through calls to deleteVisit`() = runBlocking {
        val storage = storage!!
        val writer = writer!!
        storage.deleteVisit("http://www.firefox.com", 123L)
        verify(writer, never()).deleteEverything()
        verify(writer, never()).deleteVisitsBetween(ArgumentMatchers.anyLong(), ArgumentMatchers.anyLong())
        verify(writer, never()).deleteVisitsSince(ArgumentMatchers.anyLong())
        verify(writer, never()).deletePlace("http://www.firefox.com")
        verify(writer).deleteVisit("http://www.firefox.com", 123L)
        Unit
    }

    @Test
    fun `storage passes through calls to cleanup`() {
        val storage = storage!!
        val conn = conn!!

        verify(conn, never()).close()

        storage.cleanup()
        verify(conn, times(1)).close()
    }

    @Test
    fun `storage passes through calls to runMaintanence`() = runBlocking {
        val storage = storage!!
        val writer = writer!!

        verify(writer, never()).runMaintenance()

        storage.runMaintenance()
        verify(writer, times(1)).runMaintenance()
    }

    @Test
    fun `storage passes through calls to prune`() = runBlocking {
        val storage = storage!!
        val writer = writer!!

        verify(writer, never()).pruneDestructively()

        storage.prune()
        verify(writer, times(1)).pruneDestructively()
    }
}
