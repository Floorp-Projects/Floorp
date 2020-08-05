/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import androidx.appcompat.content.res.AppCompatResources
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentInteractor
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkPayload

class BookmarkFolderViewHolderTest {

    @MockK
    private lateinit var interactor: BookmarkFragmentInteractor
    @MockK(relaxed = true)
    private lateinit var siteItemView: LibrarySiteItemView
    @MockK(relaxed = true)
    private lateinit var selectionHolder: SelectionHolder<BookmarkNode>
    private lateinit var holder: BookmarkFolderViewHolder

    private val folder = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        guid = "456",
        parentGuid = "123",
        position = 0,
        title = "Folder",
        url = null,
        children = listOf()
    )

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(any(), any()) } returns mockk(relaxed = true)

        holder = BookmarkFolderViewHolder(siteItemView, interactor, selectionHolder)
    }

    @Test
    fun `binds title and selected state`() {
        holder.bind(folder, BookmarkFragmentState.Mode.Normal())

        verify {
            siteItemView.titleView.text = folder.title
            siteItemView.overflowView.showAndEnable()
            siteItemView.changeSelected(false)
        }

        every { selectionHolder.selectedItems } returns setOf(folder)
        holder.bind(folder, BookmarkFragmentState.Mode.Selecting(setOf(folder)))

        verify {
            siteItemView.titleView.text = folder.title
            siteItemView.overflowView.hideAndDisable()
            siteItemView.changeSelected(true)
        }
    }

    @Test
    fun `bind with payload of no changes does not rebind views`() {
        holder.bind(
            folder,
            BookmarkFragmentState.Mode.Normal(),
            BookmarkPayload(false, false, false, false)
        )

        verify(inverse = true) {
            siteItemView.titleView.text = folder.title
            siteItemView.overflowView.showAndEnable()
            siteItemView.overflowView.hideAndDisable()
            siteItemView.changeSelected(any())
        }
    }
}
