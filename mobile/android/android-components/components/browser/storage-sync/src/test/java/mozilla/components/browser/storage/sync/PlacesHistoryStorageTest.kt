/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.eq
import org.junit.Test
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.times
import org.mockito.Mockito.never
import org.mozilla.places.ReadablePlacesConnectionInterface
import org.mozilla.places.SearchResult
import org.mozilla.places.VisitObservation
import org.mozilla.places.WritablePlacesConnectionInterface
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.lang.IllegalArgumentException

@RunWith(RobolectricTestRunner::class)
class PlacesHistoryStorageTest {
    private var conn: Connection? = null
    private var reader: ReadablePlacesConnectionInterface? = null
    private var writer: WritablePlacesConnectionInterface? = null

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
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.LINK)
        )

        storage.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.RELOAD)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.TYPED)
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = org.mozilla.places.VisitType.TYPED)
        )
    }

    @Test
    fun `storage passes through recordObservation calls`() = runBlocking {
        val storage = storage!!
        val writer = writer!!

        storage.recordObservation("http://www.mozilla.org", PageObservation(title = "Mozilla"))
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.UPDATE_PLACE, title = "Mozilla")
        )

        storage.recordObservation("http://www.firefox.com", PageObservation(title = null))
        verify(writer, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = org.mozilla.places.VisitType.UPDATE_PLACE, title = null)
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
            override fun reader(): ReadablePlacesConnectionInterface {
                fail()
                return mock()
            }

            override fun writer(): WritablePlacesConnectionInterface {
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
            override fun reader(): ReadablePlacesConnectionInterface {
                fail()
                return mock()
            }

            override fun writer(): WritablePlacesConnectionInterface {
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
            override fun reader(): ReadablePlacesConnectionInterface {
                fail()
                return mock()
            }

            override fun writer(): WritablePlacesConnectionInterface {
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
    fun `storage passes through calls to cleanup`() {
        val storage = storage!!
        val conn = conn!!

        verify(conn, never()).close()

        storage.cleanup()
        verify(conn, times(1)).close()
    }
}
