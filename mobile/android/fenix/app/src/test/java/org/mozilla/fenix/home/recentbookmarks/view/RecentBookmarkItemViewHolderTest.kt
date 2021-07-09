/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import kotlinx.android.synthetic.main.recent_bookmark_item.view.*
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentBookmarkItemViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: SessionControlInteractor

    private val bookmarkNoUrl = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = "Bookmark Title",
        url = null,
        children = null
    )

    private val bookmarkWithUrl = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = "Other Bookmark Title",
        url = "https://www.example.com",
        children = null
    )

    private val bookmarkNoTitle = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = null,
        url = "https://www.github.com",
        children = null
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(RecentBookmarkItemViewHolder.LAYOUT_ID, null)

        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a bookmark exists in the list THEN set the title text and subtitle from item`() {
        RecentBookmarkItemViewHolder(view, interactor).bind(bookmarkWithUrl)

        val hostFromUrl = bookmarkWithUrl.url?.tryGetHostFromUrl()

        Assert.assertEquals(bookmarkWithUrl.title, view.bookmark_title.text)
        Assert.assertEquals(hostFromUrl, view.bookmark_subtitle.text)
    }

    @Test
    fun `WHEN there is no url for the bookmark THEN do not load an icon `() {
        val viewHolder = RecentBookmarkItemViewHolder(view, interactor)

        Assert.assertNull(view.favicon_image.drawable)

        viewHolder.bind(bookmarkNoUrl)

        Assert.assertNull(view.favicon_image.drawable)
    }

    @Test
    fun `WHEN a bookmark does not have a title THEN show the url`() {
        RecentBookmarkItemViewHolder(view, interactor).bind(bookmarkNoTitle)

        Assert.assertEquals(bookmarkNoTitle.url, view.bookmark_title.text)
    }
}
