/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.widget.FrameLayout
import androidx.core.view.isInvisible
import androidx.recyclerview.widget.RecyclerView
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.collection_tab_list_row.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.Tab

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationTabListAdapterTest {

    private lateinit var interactor: CollectionCreationInteractor
    private lateinit var adapter: CollectionCreationTabListAdapter

    @Before
    fun setup() {
        interactor = mockk()
        adapter = CollectionCreationTabListAdapter(interactor)

        every { interactor.selectCollection(any(), any()) } just Runs
    }

    @Test
    fun `getItemCount should return the number of tab collections`() {
        val tab = mockk<Tab>()

        assertEquals(0, adapter.itemCount)

        adapter.updateData(
            tabs = listOf(tab),
            selectedTabs = emptySet()
        )
        assertEquals(1, adapter.itemCount)
    }

    @Test
    fun `creates and binds viewholder`() {
        val tab = mockk<Tab> {
            every { sessionId } returns "abc"
            every { title } returns "Mozilla"
            every { hostname } returns "mozilla.org"
            every { url } returns "https://mozilla.org"
        }
        adapter.updateData(
            tabs = listOf(tab),
            selectedTabs = emptySet()
        )

        val holder = adapter.createViewHolder(FrameLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        assertEquals("Mozilla", holder.tab_title.text)
        assertEquals("mozilla.org", holder.hostname.text)
        assertFalse(holder.tab_selected_checkbox.isInvisible)
        assertTrue(holder.itemView.isClickable)
    }

    @Test
    fun `updateData inserts item`() {
        val tab = mockk<Tab> {
            every { sessionId } returns "abc"
        }
        val observer = mockk<RecyclerView.AdapterDataObserver>(relaxed = true)
        adapter.registerAdapterDataObserver(observer)
        adapter.updateData(
            tabs = listOf(tab),
            selectedTabs = emptySet()
        )

        verify { observer.onItemRangeInserted(0, 1) }
    }
}
