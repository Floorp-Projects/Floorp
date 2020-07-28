/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder

class BookmarkTouchHelperTest {

    @RelaxedMockK private lateinit var interactor: BookmarkViewInteractor
    @RelaxedMockK private lateinit var viewHolder: BookmarkNodeViewHolder
    @RelaxedMockK private lateinit var item: BookmarkNode
    private lateinit var touchCallback: BookmarkTouchCallback

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        touchCallback = BookmarkTouchCallback(interactor)

        every { viewHolder.item } returns item
    }

    @Test
    fun `swiping an item calls onDelete`() {
        touchCallback.onSwiped(viewHolder, ItemTouchHelper.LEFT)

        verify {
            interactor.onDelete(setOf(item))
        }
    }

    @Test
    fun `swiping a folder calls onDelete and notifies the adapter of the change`() {
        val adapter: RecyclerView.Adapter<BookmarkNodeViewHolder> = mockk(relaxed = true)

        every { item.type } returns BookmarkNodeType.FOLDER
        every { viewHolder.bindingAdapter } returns adapter
        every { viewHolder.bindingAdapterPosition } returns 0

        touchCallback.onSwiped(viewHolder, ItemTouchHelper.LEFT)

        verify {
            interactor.onDelete(setOf(item))
            adapter.notifyItemChanged(0)
        }
    }
}
