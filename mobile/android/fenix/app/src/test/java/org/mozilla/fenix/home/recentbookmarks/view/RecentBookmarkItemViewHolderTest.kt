/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.LayoutInflater
import io.mockk.mockk
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.RecentBookmarkItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentBookmarkItemViewHolderTest {

    private lateinit var binding: RecentBookmarkItemBinding
    private lateinit var interactor: SessionControlInteractor

    private val bookmarkNoUrl = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = "Bookmark Title",
        url = null,
        dateAdded = 0,
        children = null
    )

    private val bookmarkWithUrl = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = "Other Bookmark Title",
        url = "https://www.example.com",
        dateAdded = 0,
        children = null
    )

    private val bookmarkNoTitle = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = null,
        url = "https://www.github.com",
        dateAdded = 0,
        children = null
    )

    @Before
    fun setup() {
        binding = RecentBookmarkItemBinding.inflate(LayoutInflater.from(testContext))

        interactor = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN a bookmark exists in the list THEN set the title text and subtitle from item`() {
        RecentBookmarkItemViewHolder(binding.root, interactor).bind(bookmarkWithUrl)

        val hostFromUrl = bookmarkWithUrl.url?.tryGetHostFromUrl()

        Assert.assertEquals(bookmarkWithUrl.title, binding.bookmarkTitle.text)
        Assert.assertEquals(hostFromUrl, binding.bookmarkSubtitle.text)
    }

    @Test
    fun `WHEN there is no url for the bookmark THEN do not load an icon `() {
        val viewHolder = RecentBookmarkItemViewHolder(binding.root, interactor)

        Assert.assertNull(binding.faviconImage.drawable)

        viewHolder.bind(bookmarkNoUrl)

        Assert.assertNull(binding.faviconImage.drawable)
    }

    @Test
    fun `WHEN a bookmark does not have a title THEN show the url`() {
        RecentBookmarkItemViewHolder(binding.root, interactor).bind(bookmarkNoTitle)

        Assert.assertEquals(bookmarkNoTitle.url, binding.bookmarkTitle.text)
    }
}
