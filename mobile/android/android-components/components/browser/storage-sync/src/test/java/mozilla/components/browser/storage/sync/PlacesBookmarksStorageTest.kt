/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.appservices.places.uniffi.PlacesApiException
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.TimeUnit

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class PlacesBookmarksStorageTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var bookmarks: PlacesBookmarksStorage

    @Before
    fun setup() = runTestOnMain {
        bookmarks = PlacesBookmarksStorage(testContext)
        // There's a database on disk which needs to be cleaned up between tests.
        bookmarks.writer.deleteEverything()
    }

    @After
    @Suppress("DEPRECATION")
    fun cleanup() = runTestOnMain {
        bookmarks.cleanup()
    }

    @Test
    fun `get bookmarks tree by root, recursive or not`() = runTestOnMain {
        val tree = bookmarks.getTree(BookmarkRoot.Root.id)!!
        assertEquals(BookmarkRoot.Root.id, tree.guid)
        assertNotNull(tree.children)
        assertEquals(4, tree.children!!.size)

        var children = tree.children!!.map { it.guid }
        assertTrue(BookmarkRoot.Mobile.id in children)
        assertTrue(BookmarkRoot.Unfiled.id in children)
        assertTrue(BookmarkRoot.Toolbar.id in children)
        assertTrue(BookmarkRoot.Menu.id in children)

        // Non-recursive means children of children aren't fetched.
        for (child in tree.children!!) {
            assertNull(child.children)
            assertEquals(BookmarkRoot.Root.id, child.parentGuid)
            assertEquals(BookmarkNodeType.FOLDER, child.type)
        }

        val deepTree = bookmarks.getTree(BookmarkRoot.Root.id, true)!!
        assertEquals(BookmarkRoot.Root.id, deepTree.guid)
        assertNotNull(deepTree.children)
        assertEquals(4, deepTree.children!!.size)

        children = deepTree.children!!.map { it.guid }
        assertTrue(BookmarkRoot.Mobile.id in children)
        assertTrue(BookmarkRoot.Unfiled.id in children)
        assertTrue(BookmarkRoot.Toolbar.id in children)
        assertTrue(BookmarkRoot.Menu.id in children)

        // Recursive means children of children are fetched.
        for (child in deepTree.children!!) {
            // For an empty tree, we expect to see empty lists.
            assertEquals(emptyList<BookmarkNode>(), child.children)
            assertEquals(BookmarkRoot.Root.id, child.parentGuid)
            assertEquals(BookmarkNodeType.FOLDER, child.type)
        }
    }

    @Test
    fun `bookmarks APIs smoke testing - basic operations`() = runTestOnMain {
        val url = "http://www.mozilla.org"

        assertEquals(emptyList<BookmarkNode>(), bookmarks.getBookmarksWithUrl(url))
        assertEquals(emptyList<BookmarkNode>(), bookmarks.searchBookmarks("mozilla"))

        val insertedItem = bookmarks.addItem(BookmarkRoot.Mobile.id, url, "Mozilla", 5u)

        with(bookmarks.getBookmarksWithUrl(url)) {
            assertEquals(1, this.size)
            with(this[0]) {
                assertEquals(insertedItem, this.guid)
                assertEquals(BookmarkNodeType.ITEM, this.type)
                assertEquals("Mozilla", this.title)
                assertEquals(BookmarkRoot.Mobile.id, this.parentGuid)
                // Clamped to actual range. 'Mobile' was empty, so we get 0 back.
                assertEquals(0u, this.position)
                assertEquals("http://www.mozilla.org/", this.url)
            }
        }

        val folderGuid = bookmarks.addFolder(BookmarkRoot.Mobile.id, "Test Folder", null)
        bookmarks.updateNode(
            insertedItem,
            BookmarkInfo(
                parentGuid = folderGuid,
                title = null,
                position = 9999u,
                url = null,
            ),
        )
        with(bookmarks.getBookmarksWithUrl(url)) {
            assertEquals(1, this.size)
            with(this[0]) {
                assertEquals(insertedItem, this.guid)
                assertEquals(BookmarkNodeType.ITEM, this.type)
                assertEquals("Mozilla", this.title)
                assertEquals(folderGuid, this.parentGuid)
                assertEquals(0u, this.position)
                assertEquals("http://www.mozilla.org/", this.url)
            }
        }

        val separatorGuid = bookmarks.addSeparator(folderGuid, 1u)
        with(bookmarks.getTree(folderGuid)!!) {
            assertEquals(2, this.children!!.size)
            assertEquals(BookmarkNodeType.SEPARATOR, this.children!![1].type)
        }

        assertTrue(bookmarks.deleteNode(separatorGuid))
        with(bookmarks.getTree(folderGuid)!!) {
            assertEquals(1, this.children!!.size)
            assertEquals(BookmarkNodeType.ITEM, this.children!![0].type)
        }

        with(bookmarks.searchBookmarks("mozilla")) {
            assertEquals(1, this.size)
            assertEquals("http://www.mozilla.org/", this[0].url)
        }

        with(bookmarks.getBookmark(folderGuid)!!) {
            assertEquals(folderGuid, this.guid)
            assertEquals("Test Folder", this.title)
            assertEquals(BookmarkRoot.Mobile.id, this.parentGuid)
        }

        with(bookmarks.getRecentBookmarks(1)) {
            assertEquals(insertedItem, this[0].guid)
        }

        with(bookmarks.getRecentBookmarks(1, TimeUnit.DAYS.toMillis(1))) {
            assertEquals(insertedItem, this[0].guid)
        }

        with(bookmarks.getRecentBookmarks(1, 99, System.currentTimeMillis() + 100)) {
            assertTrue(this.isEmpty())
        }

        val secondInsertedItem = bookmarks.addItem(BookmarkRoot.Unfiled.id, url, "Mozilla", 6u)

        with(bookmarks.getRecentBookmarks(2)) {
            assertEquals(secondInsertedItem, this[0].guid)
            assertEquals(insertedItem, this[1].guid)
        }

        with(bookmarks.getRecentBookmarks(2, TimeUnit.DAYS.toMillis(1))) {
            assertEquals(secondInsertedItem, this[0].guid)
            assertEquals(insertedItem, this[1].guid)
        }

        with(bookmarks.getRecentBookmarks(2, 99, System.currentTimeMillis() + 100)) {
            assertTrue(this.isEmpty())
        }

        assertTrue(bookmarks.deleteNode(secondInsertedItem))
        assertTrue(bookmarks.deleteNode(folderGuid))

        for (
        root in listOf(
            BookmarkRoot.Mobile,
            BookmarkRoot.Root,
            BookmarkRoot.Menu,
            BookmarkRoot.Toolbar,
            BookmarkRoot.Unfiled,
        )
        ) {
            try {
                bookmarks.deleteNode(root.id)
                fail("Expected root deletion for ${root.id} to fail")
            } catch (e: PlacesApiException.InvalidBookmarkOperation) {}
        }

        with(bookmarks.searchBookmarks("mozilla")) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `GIVEN bookmarks exist WHEN asked for autocomplete suggestions THEN return the first matching bookmark`() = runTest {
        bookmarks.apply {
            addItem(BookmarkRoot.Mobile.id, "https://www.mozilla.org/en-us/firefox", "Mozilla", 5u)
            addItem(BookmarkRoot.Toolbar.id, "https://support.mozilla.org/", "Support", 2u)
        }

        // Try querying for a bookmarks that doesn't exist
        var suggestion = bookmarks.getAutocompleteSuggestion("test")
        assertNull(suggestion)

        // And now for ones that do exist
        suggestion = bookmarks.getAutocompleteSuggestion("moz")
        assertNotNull(suggestion)
        assertEquals("moz", suggestion?.input)
        // There are multiple bookmarks from the mozilla host with no guarantee about the read order.
        // Use a smaller URL that would match all.
        assertTrue(suggestion?.text?.startsWith("mozilla.org/en-us/") ?: false)
        assertTrue(suggestion?.url?.startsWith("https://www.mozilla.org/en-us/") ?: false)
        assertEquals(BOOKMARKS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)

        suggestion = bookmarks.getAutocompleteSuggestion("sup")
        assertNotNull(suggestion)
        assertEquals("sup", suggestion?.input)
        assertEquals("support.mozilla.org/", suggestion?.text)
        assertEquals("https://support.mozilla.org/", suggestion?.url)
        assertEquals(BOOKMARKS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)
    }
}
