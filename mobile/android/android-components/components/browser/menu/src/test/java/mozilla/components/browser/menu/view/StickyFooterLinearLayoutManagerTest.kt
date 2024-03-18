/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class StickyFooterLinearLayoutManagerTest {
    private lateinit var manager: StickyFooterLinearLayoutManager<FakeStickyItemsAdapter>

    @Before
    fun setup() {
        manager = StickyFooterLinearLayoutManager(mock(), false)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position smaller than stickyItemPosition THEN will scroll to after that`() {
        manager.stickyItemPosition = 5
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(4, 22, scrollCallback)
        Assert.assertEquals(5, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)

        manager.scrollToIndicatedPositionWithOffset(0, 22, scrollCallback)
        Assert.assertEquals(1, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position smaller than stickyItemPosition which is displayed THEN will scroll to that`() {
        manager = spy(manager)
        doReturn(mock<View>()).`when`(manager).getChildAt(ArgumentMatchers.anyInt())
        manager.stickyItemPosition = 5
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(4, 22, scrollCallback)
        Assert.assertEquals(4, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)

        manager.scrollToIndicatedPositionWithOffset(0, 22, scrollCallback)
        Assert.assertEquals(0, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position equal to stickyItemPosition THEN will scroll to that position`() {
        manager.stickyItemPosition = 6
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(6, 22, scrollCallback)
        Assert.assertEquals(6, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN scrollToIndicatedPositionWithOffset WHEN called with a position bigger than stickyItemPosition THEN will scroll to that position`() {
        manager.stickyItemPosition = 6
        var positionToScrollResult = -1
        var offsetToScrollResult = -1
        val scrollCallback: (Int, Int) -> Unit = { position, offset ->
            positionToScrollResult = position
            offsetToScrollResult = offset
        }

        manager.scrollToIndicatedPositionWithOffset(7, 22, scrollCallback)
        Assert.assertEquals(7, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)

        manager.scrollToIndicatedPositionWithOffset(10, 22, scrollCallback)
        Assert.assertEquals(10, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)

        // Negative positions are handled by Android'd LayoutManager. We should pass any to it.
        manager.scrollToIndicatedPositionWithOffset(3333, 22, scrollCallback)
        Assert.assertEquals(3333, positionToScrollResult)
        Assert.assertEquals(22, offsetToScrollResult)
    }

    @Test
    fun `GIVEN stickyItemPosition not set WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns false`() {
        manager.stickyItemPosition = RecyclerView.NO_POSITION

        Assert.assertFalse(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN sticky item shown WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it checks the item above the sticky one`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 5
        manager.stickyItemView = mock()
        doReturn(10).`when`(manager).childCount

        manager.shouldStickyItemBeShownForCurrentPosition()

        verify(manager).getAdapterPositionForItemIndex(8)
    }

    @Test
    fun `GIVEN sticky item not shown WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it checks the bottom most item`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 5
        doReturn(10).`when`(manager).childCount

        manager.shouldStickyItemBeShownForCurrentPosition()

        verify(manager).getAdapterPositionForItemIndex(9)
    }

    @Test
    fun ` GIVEN sticky item being the last shown item WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns true`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 5
        doReturn(5).`when`(manager).getAdapterPositionForItemIndex(ArgumentMatchers.anyInt())

        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun ` GIVEN sticky item being scrolled upwards from the bottom WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns false`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 5

        doReturn(6).`when`(manager).getAdapterPositionForItemIndex(ArgumentMatchers.anyInt())
        assertFalse(manager.shouldStickyItemBeShownForCurrentPosition())

        doReturn(60).`when`(manager).getAdapterPositionForItemIndex(ArgumentMatchers.anyInt())
        assertFalse(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun ` GIVEN sticky item being scrolled downwards offscreen WHEN shouldStickyItemBeShownForCurrentPosition is called THEN it returns true`() {
        val manager = spy(manager)
        manager.stickyItemPosition = 5

        doReturn(4).`when`(manager).getAdapterPositionForItemIndex(ArgumentMatchers.anyInt())
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())

        doReturn(0).`when`(manager).getAdapterPositionForItemIndex(ArgumentMatchers.anyInt())
        assertTrue(manager.shouldStickyItemBeShownForCurrentPosition())
    }

    @Test
    fun `GIVEN a default layout menu WHEN getY is called THEN it returns the translation needed to push the sticky item to the top`() {
        manager = spy(manager)
        val stickyView: View = mock()
        doReturn(100).`when`(manager).height
        doReturn(33).`when`(stickyView).height

        Assert.assertEquals(67f, manager.getY(stickyView))
    }

    @Test
    fun `GIVEN a reverseLayout menu WHEN getY is called THEN it returns 0 as the translation to be set for the sticky item`() {
        manager = spy(StickyFooterLinearLayoutManager(mock(), true))
        doReturn(100).`when`(manager).height
        val stickyView: View = mock()
        doReturn(33).`when`(stickyView).height

        Assert.assertEquals(0f, manager.getY(stickyView))
    }
}
