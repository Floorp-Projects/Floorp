/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import junit.framework.Assert.assertEquals
import junit.framework.Assert.assertNull
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.eq
import org.junit.Test
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.times
import org.mockito.Mockito.never
import org.mozilla.places.PlacesAPI
import org.mozilla.places.SearchResult
import org.mozilla.places.VisitObservation
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.lang.IllegalArgumentException

@RunWith(RobolectricTestRunner::class)
class PlacesHistoryStorageTest {
    private var conn: Connection? = null
    private var places: PlacesAPI? = null
    private var storage: PlacesHistoryStorage? = null

    class TestablePlacesHistoryStorage(override val places: Connection) : PlacesHistoryStorage(RuntimeEnvironment.application)

    @Before
    fun setup() {
        places = mock()
        conn = mock()
        `when`(conn!!.api()).thenReturn(places)
        storage = TestablePlacesHistoryStorage(conn!!)
    }

    @Test
    fun `storage passes through recordVisit calls`() = runBlocking {
        val storage = storage!!
        val places = places!!

        storage.recordVisit("http://www.mozilla.org", VisitType.LINK)
        verify(places, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.LINK)
        )

        storage.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        verify(places, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.RELOAD)
        )

        storage.recordVisit("http://www.firefox.com", VisitType.TYPED)
        verify(places, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = org.mozilla.places.VisitType.TYPED)
        )
    }

    @Test
    fun `storage passes through recordObservation calls`() = runBlocking {
        val storage = storage!!
        val places = places!!

        storage.recordObservation("http://www.mozilla.org", PageObservation(title = "Mozilla"))
        verify(places, times(1)).noteObservation(
                VisitObservation("http://www.mozilla.org", visitType = org.mozilla.places.VisitType.UPDATE_PLACE, title = "Mozilla")
        )

        storage.recordObservation("http://www.firefox.com", PageObservation(title = null))
        verify(places, times(1)).noteObservation(
                VisitObservation("http://www.firefox.com", visitType = org.mozilla.places.VisitType.UPDATE_PLACE, title = null)
        )
    }

    @Test
    fun `storage passes through getVisited(uris) calls`() = runBlocking {
        val storage = storage!!
        val places = places!!

        storage.getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))
        verify(places, times(1)).getVisited(listOf("http://www.mozilla.org", "http://www.firefox.com"))

        storage.getVisited(listOf())
        verify(places, times(1)).getVisited(listOf())

        Unit
    }

    @Test
    fun `storage passes through getVisited() calls`() = runBlocking {
        val storage = storage!!
        val places = places!!

        storage.getVisited()
        verify(places, times(1)).getVisitedUrlsInRange(eq(0), ArgumentMatchers.anyLong(), eq(true))
        Unit
    }

    @Test
    fun `storage passes through getSuggestions calls`() {
        val storage = storage!!
        val places = places!!

        storage.getSuggestions("Hello!", 10)
        verify(places, times(1)).queryAutocomplete("Hello!", 10)

        storage.getSuggestions("World!", 0)
        verify(places, times(1)).queryAutocomplete("World!", 0)

        `when`(places.queryAutocomplete("mozilla", 10)).thenReturn(listOf(
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
        val places = places!!
        `when`(places.queryAutocomplete("mozilla", 100)).thenReturn(listOf(
            SearchResult("mozilla", "http://www.mozilla.org", "Mozilla", 10),
            SearchResult("mozilla", "http://www.firefox.com", "Mozilla Firefox", 5),
            SearchResult("mozilla", "https://en.wikipedia.org/wiki/Mozilla", "", 8))
        )
        val res = storage.getAutocompleteSuggestion("mozilla")!!
        assertEquals(3, res.totalItems)
        assertEquals("http://www.mozilla.org", res.url)
        assertEquals("mozilla.org", res.text)
        assertEquals("placesHistory", res.source)

        assertNull(storage.getAutocompleteSuggestion("hello"))
    }

    @Test()
    fun `storage passes through calls to cleanup`() {
        val storage = storage!!
        val conn = conn!!

        verify(conn, never()).close()

        storage.cleanup()
        verify(conn, times(1)).close()
    }
}
