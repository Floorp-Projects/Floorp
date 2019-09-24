/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import assertk.assertThat
import assertk.assertions.isEqualTo
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.components
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class DesktopFoldersTest {

    private lateinit var context: Context
    private lateinit var bookmarksStorage: PlacesBookmarksStorage

    private val basicNode = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = BookmarkRoot.Root.id,
        parentGuid = null,
        title = BookmarkRoot.Root.name,
        position = 0,
        url = null,
        children = null
    )

    @Before
    fun setup() {
        context = spyk(testContext)
        bookmarksStorage = mockk()
        every { context.components.core.bookmarksStorage } returns bookmarksStorage
        every { context.components.backgroundServices.accountManager.authenticatedAccount() } returns null
    }

    @Test
    fun `withRootTitle and do showMobileRoot`() {
        val desktopFolders = DesktopFolders(context, showMobileRoot = true)

        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("root")).title)
            .isEqualTo(testContext.getString(R.string.library_bookmarks))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("mobile")).title)
            .isEqualTo(testContext.getString(R.string.library_bookmarks))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("menu")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_menu))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("toolbar")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_toolbar))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("unfiled")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_unfiled))
    }

    @Test
    fun `withRootTitle and do not showMobileRoot`() {
        val desktopFolders = DesktopFolders(context, showMobileRoot = false)

        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("root")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_root))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("mobile")))
            .isEqualTo(mockNodeWithTitle("mobile"))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("menu")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_menu))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("toolbar")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_toolbar))
        assertThat(desktopFolders.withRootTitle(mockNodeWithTitle("unfiled")).title)
            .isEqualTo(testContext.getString(R.string.library_desktop_bookmarks_unfiled))
    }

    @Test
    fun `withOptionalDesktopFolders mobile node and logged out`() = runBlocking {
        every { context.components.backgroundServices.accountManager.authenticatedAccount() } returns null
        val node = basicNode.copy(guid = BookmarkRoot.Mobile.id, title = BookmarkRoot.Mobile.name)
        val desktopFolders = DesktopFolders(context, showMobileRoot = true)

        assertSame(node, desktopFolders.withOptionalDesktopFolders(node))
    }

    @Test
    fun `withOptionalDesktopFolders other node`() = runBlocking {
        val node = basicNode.copy(guid = "12345")
        val desktopFolders = DesktopFolders(context, showMobileRoot = true)

        assertSame(node, desktopFolders.withOptionalDesktopFolders(node))
    }

    private fun mockNodeWithTitle(title: String) = basicNode.copy(title = title)
}
