/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.LayoutInflater
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TabTouchCallbackTest {

    @Test
    fun `onSwiped notifies observers`() {
        val observer: TabsTray.Observer = mock()
        val observable: Observable<TabsTray.Observer> = ObserverRegistry()
        observable.register(observer)

        val touchCallback = TabTouchCallback(observable)
        val viewHolder: TabViewHolder = mock()

        touchCallback.onSwiped(viewHolder, 0)

        verify(observer, never()).onTabClosed(any())

        // With a session available.
        `when`(viewHolder.session).thenReturn(mock())

        touchCallback.onSwiped(viewHolder, 0)

        verify(observer).onTabClosed(any())
    }

    @Test
    fun `onChildDraw alters alpha of ViewHolder on swipe gesture`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = TabViewHolder(view, TabViewHolderTest.mockTabsTrayWithStyles())
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
