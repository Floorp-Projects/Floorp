/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.LayoutInflater
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class TabTouchCallbackTest {

    @Test
    fun `onSwiped notifies observers`() {
        var onTabClosedWasCalled = false

        val onTabClosed: (TabSessionState) -> Unit = {
            onTabClosedWasCalled = true
        }
        val touchCallback = TabTouchCallback(onTabClosed)
        val viewHolder: TabViewHolder = mock()

        touchCallback.onSwiped(viewHolder, 0)

        assertFalse(onTabClosedWasCalled)

        // With a session available.
        `when`(viewHolder.tab).thenReturn(mock())

        touchCallback.onSwiped(viewHolder, 0)

        assertTrue(onTabClosedWasCalled)
    }

    @Test
    fun `onChildDraw alters alpha of ViewHolder on swipe gesture`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view)
        val callback = TabTouchCallback(mock())

        holder.itemView.alpha = 0f

        callback.onChildDraw(mock(), mock(), holder, 0f, 0f, ItemTouchHelper.ACTION_STATE_DRAG, true)

        assertEquals(0f, holder.itemView.alpha)

        callback.onChildDraw(mock(), mock(), holder, 0f, 0f, ItemTouchHelper.ACTION_STATE_SWIPE, true)

        assertEquals(1f, holder.itemView.alpha)
    }

    @Test
    fun `alpha default is full`() {
        val touchCallback = TabTouchCallback(mock())
        assertEquals(1f, touchCallback.alphaForItemSwipe(0f, 0))
    }

    @Test
    fun `onMove is not implemented`() {
        val touchCallback = TabTouchCallback(mock())
        assertFalse(touchCallback.onMove(mock(), mock(), mock()))
    }
}
