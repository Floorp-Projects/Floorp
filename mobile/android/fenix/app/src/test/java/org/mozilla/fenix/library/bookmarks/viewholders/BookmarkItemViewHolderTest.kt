/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentInteractor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkPayload

class BookmarkItemViewHolderTest {

    @MockK
    private lateinit var interactor: BookmarkFragmentInteractor

    @MockK(relaxed = true)
    private lateinit var siteItemView: LibrarySiteItemView

    private lateinit var holder: BookmarkItemViewHolder

    private val item = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "456",
        parentGuid = "123",
        position = 0,
        title = "Mozilla",
        url = "https://www.mozilla.org",
        children = listOf()
    )

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        holder = BookmarkItemViewHolder(siteItemView, interactor)
    }

    @Test
    fun `binds views for unselected item`() {
        val mode = BookmarkFragmentState.Mode.Normal()
        holder.bind(item, mode)

        verify {
            siteItemView.setSelectionInteractor(item, mode, interactor)
            siteItemView.titleView.text = item.title
            siteItemView.urlView.text = item.url
            siteItemView.overflowView.showAndEnable()
            siteItemView.changeSelected(false)
            holder.setColorsAndIcons(item.url)
        }
    }

    @Test
    fun `binds views for selected item`() {
        val mode = BookmarkFragmentState.Mode.Selecting(setOf(item))
        holder.bind(item, mode)

        verify {
            siteItemView.setSelectionInteractor(item, mode, interactor)
            siteItemView.titleView.text = item.title
            siteItemView.urlView.text = item.url
            siteItemView.overflowView.hideAndDisable()
            siteItemView.changeSelected(true)
            holder.setColorsAndIcons(item.url)
        }
    }

    @Test
    fun `bind with payload of no changes does not rebind views`() {
        holder.bind(
            item,
            BookmarkFragmentState.Mode.Normal(),
            BookmarkPayload(false, false, false, false)
        )

        verify(inverse = true) {
            siteItemView.titleView.text = item.title
            siteItemView.urlView.text = item.url
            siteItemView.overflowView.showAndEnable()
            siteItemView.overflowView.hideAndDisable()
            siteItemView.changeSelected(any())
            holder.setColorsAndIcons(item.url)
        }
    }

    @Test
    fun `binding an item with a null title uses the url as the title`() {
        val item = item.copy(title = null)
        holder.bind(item, BookmarkFragmentState.Mode.Normal())

        verify { siteItemView.titleView.text = item.url }
    }

    @Test
    fun `binding an item with a blank title uses the url as the title`() {
        val item = item.copy(title = " ")
        holder.bind(item, BookmarkFragmentState.Mode.Normal())

        verify { siteItemView.titleView.text = item.url }
    }

    @Test
    fun `rebinds title if item title is null and the item url has changed`() {
        val item = item.copy(title = null)
        holder.bind(
            item,
            BookmarkFragmentState.Mode.Normal(),
            BookmarkPayload(
                titleChanged = false,
                urlChanged = true,
                selectedChanged = false,
                modeChanged = false
            )
        )

        verify { siteItemView.titleView.text = item.url }
    }

    @Test
    fun `rebinds title if item title is blank and the item url has changed`() {
        val item = item.copy(title = " ")
        holder.bind(
            item,
            BookmarkFragmentState.Mode.Normal(),
            BookmarkPayload(
                titleChanged = false,
                urlChanged = true,
                selectedChanged = false,
                modeChanged = false
            )
        )

        verify { siteItemView.titleView.text = item.url }
    }
}
