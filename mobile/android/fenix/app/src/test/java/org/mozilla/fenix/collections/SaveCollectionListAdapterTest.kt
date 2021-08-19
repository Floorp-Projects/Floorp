/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.view.ViewGroup
import android.widget.FrameLayout
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.CollectionsListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SaveCollectionListAdapterTest {

    private lateinit var parent: ViewGroup
    private lateinit var interactor: CollectionCreationInteractor
    private lateinit var adapter: SaveCollectionListAdapter

    @Before
    fun setup() {
        parent = FrameLayout(testContext)
        interactor = mockk()
        adapter = SaveCollectionListAdapter(interactor)

        every { interactor.selectCollection(any(), any()) } just Runs
    }

    @Test
    fun `getItemCount should return the number of tab collections`() {
        val collection = mockk<TabCollection>()

        assertEquals(0, adapter.itemCount)

        adapter.updateData(
            tabCollections = listOf(collection),
            selectedTabs = emptySet()
        )
        assertEquals(1, adapter.itemCount)
    }

    @Test
    fun `creates and binds viewholder`() {
        val collection = mockk<TabCollection> {
            every { id } returns 0L
            every { title } returns "Collection"
            every { tabs } returns listOf(
                mockk {
                    every { url } returns "https://mozilla.org"
                },
                mockk {
                    every { url } returns "https://firefox.com"
                }
            )
        }
        adapter.updateData(
            tabCollections = listOf(collection),
            selectedTabs = emptySet()
        )

        val holder = adapter.createViewHolder(parent, 0)
        adapter.bindViewHolder(holder, 0)
        val binding = CollectionsListItemBinding.bind(holder.itemView)

        assertEquals("Collection", binding.collectionItem.text)
        assertEquals("mozilla.org, firefox.com", binding.collectionDescription.text)

        holder.itemView.performClick()
        verify { interactor.selectCollection(collection, emptyList()) }
    }
}
