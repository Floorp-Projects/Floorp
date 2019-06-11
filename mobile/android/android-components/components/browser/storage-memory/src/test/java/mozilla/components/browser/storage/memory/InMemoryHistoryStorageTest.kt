/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.memory

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.Thread.sleep

@RunWith(AndroidJUnit4::class)
class InMemoryHistoryStorageTest {

    @Test
    fun `store can be used to track visit information`() = runBlocking {
        val history = InMemoryHistoryStorage()

        assertEquals(0, history.pages.size)

        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        assertEquals(1, history.pages.size)
        assertEquals(1, history.pages["http://www.mozilla.org"]!!.size)
        assertEquals(VisitType.LINK, history.pages["http://www.mozilla.org"]!![0].type)

        // Reloads are recorded.
        history.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        assertEquals(1, history.pages.size)
        assertEquals(2, history.pages["http://www.mozilla.org"]!!.size)
        assertEquals(VisitType.LINK, history.pages["http://www.mozilla.org"]!![0].type)
        assertEquals(VisitType.RELOAD, history.pages["http://www.mozilla.org"]!![1].type)

        // Visits for multiple pages are tracked.
        history.recordVisit("http://www.firefox.com", VisitType.LINK)
        assertEquals(2, history.pages.size)
        assertEquals(2, history.pages["http://www.mozilla.org"]!!.size)
        assertEquals(VisitType.LINK, history.pages["http://www.mozilla.org"]!![0].type)
        assertEquals(VisitType.RELOAD, history.pages["http://www.mozilla.org"]!![1].type)
        assertEquals(1, history.pages["http://www.firefox.com"]!!.size)
        assertEquals(VisitType.LINK, history.pages["http://www.firefox.com"]!![0].type)
    }

    @Test
    fun `store can be used to query detailed visit information`() = runBlocking {
        val history = InMemoryHistoryStorage()

        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordVisit("http://www.mozilla.org", VisitType.RELOAD)
        history.recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
        history.recordVisit("http://www.firefox.com", VisitType.LINK)

        history.recordVisit("http://www.firefox.com", VisitType.REDIRECT_TEMPORARY)

        val visits = history.getDetailedVisits(0, excludeTypes = listOf(VisitType.REDIRECT_TEMPORARY))
        assertEquals(3, visits.size)
        assertEquals("http://www.mozilla.org", visits[0].url)
        assertEquals("Mozilla", visits[0].title)
        assertEquals(VisitType.LINK, visits[0].visitType)

        assertEquals("http://www.mozilla.org", visits[1].url)
        assertEquals("Mozilla", visits[1].title)
        assertEquals(VisitType.RELOAD, visits[1].visitType)

        assertEquals("http://www.firefox.com", visits[2].url)
        assertEquals(null, visits[2].title)
        assertEquals(VisitType.LINK, visits[2].visitType)

        val visitsAll = history.getDetailedVisits(0)
        assertEquals(4, visitsAll.size)
    }

    @Test
    fun `store can be used to record and retrieve history via webview-style callbacks`() = runBlocking {
        val history = InMemoryHistoryStorage()

        // Empty.
        assertEquals(0, history.getVisited().size)

        // Regular visits are tracked.
        history.recordVisit("https://www.mozilla.org", VisitType.LINK)
        assertEquals(listOf("https://www.mozilla.org"), history.getVisited())

        // Multiple visits can be tracked, results ordered by "URL's first seen first".
        history.recordVisit("https://www.firefox.com", VisitType.LINK)
        assertEquals(listOf("https://www.mozilla.org", "https://www.firefox.com"), history.getVisited())

        // Visits marked as reloads can be tracked.
        history.recordVisit("https://www.firefox.com", VisitType.RELOAD)
        assertEquals(listOf("https://www.mozilla.org", "https://www.firefox.com"), history.getVisited())

        // Visited urls are certainly a set.
        history.recordVisit("https://www.firefox.com", VisitType.LINK)
        history.recordVisit("https://www.mozilla.org", VisitType.LINK)
        history.recordVisit("https://www.wikipedia.org", VisitType.LINK)
        assertEquals(
            listOf("https://www.mozilla.org", "https://www.firefox.com", "https://www.wikipedia.org"),
            history.getVisited()
        )
    }

    @Test
    fun `store can be used to record and retrieve history via gecko-style callbacks`() = runBlocking {
        val history = InMemoryHistoryStorage()

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
        val history = InMemoryHistoryStorage()
        assertEquals(0, history.pageMeta.size)

        // Title changes are recorded.
        history.recordObservation("https://www.wikipedia.org", PageObservation("Wikipedia"))
        assertEquals(1, history.pageMeta.size)
        assertEquals(PageObservation("Wikipedia"), history.pageMeta["https://www.wikipedia.org"])

        history.recordObservation("https://www.wikipedia.org", PageObservation("Википедия"))
        assertEquals(1, history.pageMeta.size)
        assertEquals(PageObservation("Википедия"), history.pageMeta["https://www.wikipedia.org"])

        // Titles for different pages are recorded.
        history.recordObservation("https://www.firefox.com", PageObservation("Firefox"))
        history.recordObservation("https://www.mozilla.org", PageObservation("Мозилла"))
        assertEquals(3, history.pageMeta.size)
        assertEquals(PageObservation("Википедия"), history.pageMeta["https://www.wikipedia.org"])
        assertEquals(PageObservation("Firefox"), history.pageMeta["https://www.firefox.com"])
        assertEquals(PageObservation("Мозилла"), history.pageMeta["https://www.mozilla.org"])
    }

    @Test
    fun `store can provide suggestions`() = runBlocking {
        val history = InMemoryHistoryStorage()
        assertEquals(0, history.getSuggestions("Mozilla", 100).size)

        history.recordVisit("http://www.firefox.com", VisitType.LINK)
        val search = history.getSuggestions("Mozilla", 100)
        assertEquals(1, search.size)
        assertEquals("http://www.firefox.com", search[0].url)

        history.recordVisit("http://www.wikipedia.org", VisitType.LINK)
        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordVisit("http://www.moscow.ru", VisitType.LINK)
        history.recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
        history.recordObservation("http://www.firefox.com", PageObservation("Mozilla Firefox"))
        history.recordObservation("http://www.moscow.ru", PageObservation("Moscow City"))
        history.recordObservation("http://www.moscow.ru/notitle", PageObservation(""))

        // Empty search.
        assertEquals(5, history.getSuggestions("", 100).size)

        val search2 = history.getSuggestions("Mozilla", 100)
        assertEquals(5, search2.size)
        assertEquals("http://www.mozilla.org", search2[0].id)
        assertEquals("http://www.mozilla.org", search2[0].url)
        assertEquals("Mozilla", search2[0].title)

        assertEquals("http://www.firefox.com", search2[1].id)
        assertEquals("http://www.firefox.com", search2[1].url)
        assertEquals("Mozilla Firefox", search2[1].title)

        assertEquals("http://www.moscow.ru", search2[2].id)
        assertEquals("http://www.moscow.ru", search2[2].url)
        assertEquals("Moscow City", search2[2].title)

        assertEquals("http://www.wikipedia.org", search2[3].id)
        assertEquals("http://www.wikipedia.org", search2[3].url)
        assertEquals(null, search2[3].title)
    }

    @Test
    fun `store can provide suggestions respecting the limit`() = runBlocking {
        val history = InMemoryHistoryStorage()
        history.recordVisit("http://www.wikipedia.org", VisitType.LINK)
        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        history.recordVisit("http://www.moscow.ru", VisitType.LINK)
        history.recordObservation("http://www.mozilla.org", PageObservation("Mozilla"))
        history.recordObservation("http://www.firefox.com", PageObservation("Mozilla Firefox"))
        history.recordObservation("http://www.moscow.ru", PageObservation("Moscow"))

        assertEquals(3, history.getSuggestions("Mozilla", 3).size)
        assertEquals(2, history.getSuggestions("Mozilla", 2).size)
        assertEquals(1, history.getSuggestions("Mozilla", 1).size)

        val results = history.getSuggestions("Mozilla", 3)
        assertEquals("http://www.mozilla.org", results[0].url)
        assertEquals("http://www.moscow.ru", results[1].url)
        assertEquals("http://www.firefox.com", results[2].url)

        val results2 = history.getSuggestions("Mozilla", 1)
        assertEquals("http://www.mozilla.org", results2[0].url)
    }

    @Test
    fun `store can provide autocomplete suggestions`() = runBlocking {
        val history = InMemoryHistoryStorage()

        assertNull(history.getAutocompleteSuggestion("moz"))

        history.recordVisit("http://www.mozilla.org", VisitType.LINK)
        var res = history.getAutocompleteSuggestion("moz")!!
        assertEquals("mozilla.org", res.text)
        assertEquals("http://www.mozilla.org", res.url)
        assertEquals("memoryHistory", res.source)
        assertEquals(1, res.totalItems)

        history.recordVisit("http://firefox.com", VisitType.LINK)
        res = history.getAutocompleteSuggestion("firefox")!!
        assertEquals("firefox.com", res.text)
        assertEquals("http://firefox.com", res.url)
        assertEquals("memoryHistory", res.source)
        assertEquals(2, res.totalItems)

        history.recordVisit("https://en.wikipedia.org/wiki/Mozilla", VisitType.LINK)
        res = history.getAutocompleteSuggestion("en")!!
        assertEquals("en.wikipedia.org/wiki/mozilla", res.text)
        assertEquals("https://en.wikipedia.org/wiki/mozilla", res.url)
        assertEquals("memoryHistory", res.source)
        assertEquals(3, res.totalItems)

        assertNull(history.getAutocompleteSuggestion("hello"))
    }

    @Test
    fun `store can delete everything`() = runBlocking {
        val history = InMemoryHistoryStorage()

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
        val history = InMemoryHistoryStorage()

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
        assertEquals("http://www.firefox.com", history.getVisited()[0])

        history.deleteVisitsFor("http://www.firefox.com")
        assertEquals(0, history.getVisited().size)
    }

    @Test
    fun `store can delete by 'since'`() = runBlocking {
        val history = InMemoryHistoryStorage()

        history.recordVisit("http://www.mozilla.org", VisitType.TYPED)
        history.recordVisit("http://www.mozilla.org", VisitType.DOWNLOAD)
        history.recordVisit("http://www.mozilla.org", VisitType.BOOKMARK)

        history.deleteVisitsSince(0)
        val visits = history.getVisited()
        assertEquals(0, visits.size)
    }

    @Test
    fun `store can delete by 'range'`() {
        val history = InMemoryHistoryStorage()

        runBlocking {
            history.recordVisit("http://www.mozilla.org/1", VisitType.TYPED)
            sleep(10)
            history.recordVisit("http://www.mozilla.org/2", VisitType.DOWNLOAD)
            sleep(10)
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
        val history = InMemoryHistoryStorage()

        runBlocking {
            history.recordVisit("http://www.mozilla.org/1", VisitType.TYPED)
            sleep(10)
            history.recordVisit("http://www.mozilla.org/2", VisitType.DOWNLOAD)
            sleep(10)
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
}
