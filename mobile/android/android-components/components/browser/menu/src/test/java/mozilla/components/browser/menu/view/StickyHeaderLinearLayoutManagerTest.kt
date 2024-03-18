/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

class StickyHeaderLinearLayoutManagerTest {
    private lateinit var manager: StickyHeaderLinearLayoutManager<FakeStickyItemsAdapter>

    @Before
    fun setup() {
        manager = StickyHeaderLinearLayoutManager(mock(), false)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position bigger than stickyItemPosition THEN will scroll to before that`() {
        manager.stickyItemPosition = 5
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(6, 22, scrollCallback)
        assertEquals(5, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)

        manager.scrollToIndicatedPositionWithOffset(10, 22, scrollCallback)
        assertEquals(9, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position equal to stickyItemPosition THEN will scroll to before that`() {
        manager.stickyItemPosition = 6
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(6, 22, scrollCallback)
        assertEquals(5, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position smaller than stickyItemPosition THEN will scroll to that position`() {
        manager.stickyItemPosition = 6
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(5, 22, scrollCallback)
        assertEquals(5, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)

        manager.scrollToIndicatedPositionWithOffset(0, 22, scrollCallback)
        assertEquals(0, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)

        // Negative positions are handled by Android'd LayoutManager. We should pass any to it.
        manager.scrollToIndicatedPositionWithOffset(-33, 22, scrollCallback)
        assertEquals(-33, positionToScrollResult)
        assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN stickyItemPosition not set WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns false`() {
        manager.stickyItemPosition = RecyclerView.NO_POSITION

        assertFalse(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN the top item is the sticky one WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns true`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 3
        doReturn(3).`when`(manager).getAdapterPositionForItemIndex(0)

        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN items below the sticky item are scrolled upwards offscreen WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns true`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 3

        doReturn(4).`when`(manager).getAdapterPositionForItemIndex(0)
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())

        doReturn(5).`when`(manager).getAdapterPositionForItemIndex(0)
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN items above the sticky item shown at top but ofsetted offscreen WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns true`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 3
        doReturn(1).`when`(manager).getAdapterPositionForItemIndex(0)
        val topMostItem: View = mock()
        doReturn(topMostItem).`when`(manager).getChildAt(0)

        doReturn(0).`when`(topMostItem).bottom
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())

        doReturn(-5).`when`(topMostItem).bottom
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN the sticky item is shown below the top of the list WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns false`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 3
        doReturn(1).`when`(manager).getAdapterPositionForItemIndex(0)

        assertFalse(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN a default layout menu WHEN getY is called THEN it returns 0 as the translation to be set for the sticky item`() {
        manager = spy(manager)
        val stickyView: View = mock()
        doReturn(100).`when`(manager).height
        doReturn(33).`when`(stickyView).height

        assertEquals(0f, manager.getY(stickyView))
    }

    @Test
    fun `GIVEN a reverseLayout menu WHEN getY is called THEN it returns the translation needed to push the sticky item to the top`() {
        manager = spy(StickyHeaderLinearLayoutManager(mock(), true))
        doReturn(100).`when`(manager).height
        val stickyView: View = mock()
        doReturn(33).`when`(stickyView).height

        assertEquals(67f, manager.getY(stickyView))
    }
}
