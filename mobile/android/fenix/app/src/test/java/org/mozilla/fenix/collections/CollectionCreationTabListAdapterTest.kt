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
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.CollectionTabListRowBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.Tab

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationTabListAdapterTest {

    private lateinit var interactor: CollectionCreationInteractor
    private lateinit var adapter: CollectionCreationTabListAdapter
    private lateinit var mozillaTab: Tab

    @Before
    fun setup() {
        interactor = mockk()
        adapter = CollectionCreationTabListAdapter(interactor)
        mozillaTab = mockk {
            every { sessionId } returns "abc"
            every { title } returns "Mozilla"
            every { hostname } returns "mozilla.org"
            every { url } returns "https://mozilla.org"
        }

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
        adapter.updateData(
            tabs = listOf(mozillaTab),
            selectedTabs = emptySet(),
            hideCheckboxes = false
        )

        val holder = adapter.createViewHolder(FrameLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)
        val binding = CollectionTabListRowBinding.bind(holder.itemView)

        assertEquals("Mozilla", binding.tabTitle.text)
        assertEquals("mozilla.org", binding.hostname.text)
        assertFalse(binding.tabSelectedCheckbox.isInvisible)
        assertTrue(holder.itemView.isClickable)

        every { interactor.addTabToSelection(mozillaTab) } just Runs
        every { interactor.removeTabFromSelection(mozillaTab) } just Runs
        assertFalse(binding.tabSelectedCheckbox.isChecked)

        binding.tabSelectedCheckbox.isChecked = true
        verify { interactor.addTabToSelection(mozillaTab) }

        binding.tabSelectedCheckbox.isChecked = false
        verify { interactor.removeTabFromSelection(mozillaTab) }
    }

    @Test
    fun `creates and binds viewholder for selected tab`() {
        every { interactor.addTabToSelection(mozillaTab) } just Runs

        adapter.updateData(
            tabs = listOf(mozillaTab),
            selectedTabs = setOf(mozillaTab),
            hideCheckboxes = true
        )

        val holder = adapter.createViewHolder(FrameLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)
        val binding = CollectionTabListRowBinding.bind(holder.itemView)

        assertEquals("Mozilla", binding.tabTitle.text)
        assertEquals("mozilla.org", binding.hostname.text)
        assertTrue(binding.tabSelectedCheckbox.isInvisible)
        assertFalse(holder.itemView.isClickable)
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
