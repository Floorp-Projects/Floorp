/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.widget.FrameLayout
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class CollectionsListAdapterTest {
    private val collectionList: Array<String> =
        arrayOf(
            "Add new collection",
            "Collection 1",
            "Collection 2",
        )
    private val onNewCollectionClicked: () -> Unit = mockk(relaxed = true)

    @Test
    fun `getItemCount should return the correct list size`() {
        val adapter = CollectionsListAdapter(collectionList, onNewCollectionClicked)

        assertEquals(3, adapter.itemCount)
    }

    @Test
    fun `getSelectedCollection should account for add new collection when returning right item`() {
        val adapter = CollectionsListAdapter(collectionList, onNewCollectionClicked)

        // first collection by default
        assertEquals(1, adapter.checkedPosition)
        assertEquals(0, adapter.getSelectedCollection())

        adapter.checkedPosition = 3
        assertEquals(2, adapter.getSelectedCollection())
    }

    @Test
    fun `creates and binds viewholder`() {
        val adapter = CollectionsListAdapter(collectionList, onNewCollectionClicked)

        val holder1 = adapter.createViewHolder(FrameLayout(testContext), 0)
        val holder2 = adapter.createViewHolder(FrameLayout(testContext), 0)
        val holder3 = adapter.createViewHolder(FrameLayout(testContext), 0)

        adapter.bindViewHolder(holder1, 0)
        adapter.bindViewHolder(holder2, 1)
        adapter.bindViewHolder(holder3, 2)

        assertEquals("Add new collection", holder1.textView.text)
        holder1.textView.callOnClick()
        verify {
            onNewCollectionClicked()
        }

        assertEquals(true, holder2.textView.isChecked)
        assertEquals("Collection 1", holder2.textView.text)
        holder2.textView.callOnClick()
        assertEquals(true, holder2.textView.isChecked)

        assertEquals(false, holder3.textView.isChecked)
        assertEquals("Collection 2", holder3.textView.text)
        holder3.textView.callOnClick()
        adapter.bindViewHolder(holder3, 2)
        assertEquals(true, holder3.textView.isChecked)
    }
}
