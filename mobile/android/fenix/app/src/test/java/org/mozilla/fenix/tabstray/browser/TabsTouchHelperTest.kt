/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.widget.FrameLayout
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.ItemTouchHelper.ACTION_STATE_IDLE
import androidx.recyclerview.widget.ItemTouchHelper.Callback.makeMovementFlags
import androidx.recyclerview.widget.RecyclerView
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.viewholders.SyncedTabsPageViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class TabsTouchHelperTest {

    @Test
    fun `movement flags remain unchanged if onSwipeToDelete is true`() {
        val recyclerView = RecyclerView(testContext)
        val layout = FrameLayout(testContext)
        val viewHolder = SyncedTabsPageViewHolder(layout, mockk())
        val callback = TouchCallback(mockk(), { true }, mockk())

        assertEquals(0, callback.getDragDirs(recyclerView, viewHolder))
        assertEquals(
            ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT,
            callback.getSwipeDirs(recyclerView, viewHolder)
        )

        val actual = callback.getMovementFlags(recyclerView, viewHolder)
        val expected = makeMovementFlags(0, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT)

        assertEquals(expected, actual)
    }

    @Test
    fun `movement flags remain unchanged if onSwipeToDelete is false`() {
        val recyclerView = RecyclerView(testContext)
        val layout = FrameLayout(testContext)
        val viewHolder = SyncedTabsPageViewHolder(layout, mockk())
        val callback = TouchCallback(mockk(), { false }, mockk())

        assertEquals(0, callback.getDragDirs(recyclerView, viewHolder))
        assertEquals(
            ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT,
            callback.getSwipeDirs(recyclerView, viewHolder)
        )

        val actual = callback.getMovementFlags(recyclerView, viewHolder)
        val expected = ItemTouchHelper.Callback.makeFlag(ACTION_STATE_IDLE, 0)

        assertEquals(expected, actual)
    }
}
