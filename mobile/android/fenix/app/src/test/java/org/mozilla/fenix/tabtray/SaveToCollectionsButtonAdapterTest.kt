/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.widget.FrameLayout
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabtray.SaveToCollectionsButtonAdapter.Item
import org.mozilla.fenix.tabtray.SaveToCollectionsButtonAdapter.ViewHolder
import kotlin.random.Random

@RunWith(FenixRobolectricTestRunner::class)
class SaveToCollectionsButtonAdapterTest {

    private lateinit var adapter: SaveToCollectionsButtonAdapter
    private lateinit var interactor: TabTrayInteractor

    @Before
    fun setup() {
        interactor = mockk(relaxed = true)
        adapter = SaveToCollectionsButtonAdapter(interactor)
    }

    @Test
    fun `create adapter only has one item in it`() {
        assertEquals(1, adapter.itemCount)
        assertTrue(adapter.currentList.first() is Item)
    }

    @Test
    fun `viewholder click invokes interactor`() {
        val itemView = FrameLayout(testContext)
        val viewHolder = ViewHolder(itemView, interactor)

        viewHolder.onClick(itemView)

        verify { interactor.onEnterMultiselect() }
    }

    @Test
    fun `always use the same layout`() {
        assertEquals(ViewHolder.LAYOUT_ID, adapter.getItemViewType(Random.nextInt()))
    }
}
