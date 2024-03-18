/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DesktopFoldersTest {

    private lateinit var context: Context

    private val basicNode = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Root.id,
        parentGuid = null,
        title = BookmarkRoot.Root.name,
        position = 0u,
        url = null,
        dateAdded = 0,
        children = null,
    )

    @Before
    fun setup() {
        context = spyk(testContext)
        every { context.components.core.bookmarksStorage } returns mockk()
    }

    @Test
    fun `withRootTitle and do showMobileRoot`() {
        assertEquals(testContext.getString(R.string.library_bookmarks), friendlyRootTitle(context, mockNodeWithTitle("root")))
        assertEquals(testContext.getString(R.string.library_bookmarks), friendlyRootTitle(context, mockNodeWithTitle("mobile")))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_menu), friendlyRootTitle(context, mockNodeWithTitle("menu")))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_toolbar), friendlyRootTitle(context, mockNodeWithTitle("toolbar")))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_unfiled), friendlyRootTitle(context, mockNodeWithTitle("unfiled")))
    }

    @Test
    fun `withRootTitle and do not showMobileRoot`() {
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_root), friendlyRootTitle(context, mockNodeWithTitle("root"), false))
        assertEquals(mockNodeWithTitle("mobile").title, friendlyRootTitle(context, mockNodeWithTitle("mobile"), false))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_menu), friendlyRootTitle(context, mockNodeWithTitle("menu"), false))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_toolbar), friendlyRootTitle(context, mockNodeWithTitle("toolbar"), false))
        assertEquals(testContext.getString(R.string.library_desktop_bookmarks_unfiled), friendlyRootTitle(context, mockNodeWithTitle("unfiled"), false))
    }

    @Test
    fun `withOptionalDesktopFolders other node`() = runTest {
        val node = basicNode.copy(guid = "12345")
        val desktopFolders = DesktopFolders(context, showMobileRoot = true)

        assertSame(node, desktopFolders.withOptionalDesktopFolders(node))
    }

    private fun mockNodeWithTitle(title: String) = basicNode.copy(title = title)
}
