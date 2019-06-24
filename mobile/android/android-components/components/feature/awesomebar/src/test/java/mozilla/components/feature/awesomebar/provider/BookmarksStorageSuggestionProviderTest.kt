/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.test.mock
import mozilla.components.support.utils.StorageUtils.levenshteinDistance
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class BookmarksStorageSuggestionProviderTest {

    private val bookmarks = testableBookmarksStorage()

    private val newItem = BookmarkNode(
        BookmarkNodeType.ITEM, "123", "456", null,
        "Mozilla", "http://www.mozilla.org", null
    )

    @Test
    fun `Provider returns empty list when text is empty`() = runBlocking {
        val provider = BookmarksStorageSuggestionProvider(mock(), mock())

        val suggestions = provider.onInputChanged("")
        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns suggestions from configured bookmarks storage`() = runBlocking {
        val provider = BookmarksStorageSuggestionProvider(bookmarks, mock())

        val id = bookmarks.addItem("Mobile", newItem.url!!, newItem.title!!, null)

        var suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
        assertEquals(id, suggestions[0].id)

        suggestions = provider.onInputChanged("mozi")
        assertEquals(1, suggestions.size)
        assertEquals(id, suggestions[0].id)

        assertEquals("http://www.mozilla.org", suggestions[0].description)
    }

    @Test
    fun `Provider does not return duplicate suggestions`() = runBlocking {
        val provider = BookmarksStorageSuggestionProvider(bookmarks, mock())

        for (i in 1..20) {
            bookmarks.addItem("Mobile", newItem.url!!, newItem.title!!, null)
        }

        val suggestions = provider.onInputChanged("moz")
        assertEquals(1, suggestions.size)
    }

    @Test
    fun `Provider limits number of returned unique suggestions`() = runBlocking {
        val provider = BookmarksStorageSuggestionProvider(bookmarks, mock())

        for (i in 1..100) {
            bookmarks.addItem(
                "Mobile",
                "${newItem.url!!} + $i",
                newItem.title!!,
                null
            )
        }

        val suggestions = provider.onInputChanged("moz")
        assertEquals(20, suggestions.size)
    }

    @Test
    fun `Provider suggestion should get cleared when text changes`() {
        val provider = BookmarksStorageSuggestionProvider(mock(), mock())
        assertTrue(provider.shouldClearSuggestions)
    }

    @SuppressWarnings
    class testableBookmarksStorage : BookmarksStorage {
        val bookmarkMap: HashMap<String, BookmarkNode> = hashMapOf()

        override suspend fun getTree(guid: String, recursive: Boolean): BookmarkNode? {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun getBookmark(guid: String): BookmarkNode? {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun getBookmarksWithUrl(url: String): List<BookmarkNode> {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun searchBookmarks(query: String, limit: Int): List<BookmarkNode> =
            synchronized(bookmarkMap) {
                data class Hit(val key: String, val score: Int)

                val urlMatches = bookmarkMap.asSequence().map {
                    Hit(it.value.guid, levenshteinDistance(it.value.url!!, query))
                }
                val titleMatches = bookmarkMap.asSequence().map {
                    Hit(it.value.guid, levenshteinDistance(it.value.title ?: "", query))
                }
                val matchedUrls = mutableMapOf<String, Int>()
                urlMatches.plus(titleMatches).forEach {
                    if (matchedUrls.containsKey(it.key) && matchedUrls[it.key]!! < it.score) {
                        matchedUrls[it.key] = it.score
                    } else {
                        matchedUrls[it.key] = it.score
                    }
                }
                // Calculate maxScore so that we can invert our scoring.
                // Lower Levenshtein distance should produce a higher score.
                urlMatches.maxBy { it.score }?.score
                    ?: return@synchronized listOf()

                // TODO exclude non-matching results entirely? Score that implies complete mismatch.
                matchedUrls.asSequence().sortedBy { it.value }.map {
                    bookmarkMap[it.key]!!
                }.take(limit).toList()
            }

        override suspend fun addItem(
            parentGuid: String,
            url: String,
            title: String,
            position: Int?
        ): String {
            val id = UUID.randomUUID().toString()
            bookmarkMap[id] =
                BookmarkNode(BookmarkNodeType.ITEM, id, parentGuid, position, title, url, null)
            return id
        }

        override suspend fun addFolder(parentGuid: String, title: String, position: Int?): String {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun addSeparator(parentGuid: String, position: Int?): String {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun updateNode(guid: String, info: BookmarkInfo) {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun deleteNode(guid: String): Boolean {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override suspend fun runMaintenance() {
            // "Not needed for the test"
            throw NotImplementedError()
        }

        override fun cleanup() {
            // "Not needed for the test"
            throw NotImplementedError()
        }
    }
}
