/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.graphics.PointF
import android.view.View
import android.view.ViewTreeObserver
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager.INVALID_OFFSET
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class StickyItemsLinearLayoutManagerTest {
    // For shorter test names "StickyItemsLinearLayoutManager" is referred to as SILLM.

    private lateinit var manager: FakeStickyItemLayoutManager<FakeStickyItemsAdapter>

    @Before
    fun setup() {
        manager = FakeStickyItemLayoutManager(mock())
    }

    @Test
    fun `GIVEN a SILLM WHEN a new instance is contructed THEN it has specific default values`() {
        assertEquals(RecyclerView.NO_POSITION, manager.stickyItemPosition)
        assertEquals(RecyclerView.NO_POSITION, manager.scrollPosition)
        assertEquals(0, manager.scrollOffset)
    }

    @Test
    fun `GIVEN a SILLM WHEN onAttachedToWindow called THEN it calls super and sets the new adapter`() {
        manager = spy(manager)
        val list = Mockito.mock(RecyclerView::class.java)

        manager.onAttachedToWindow(list)

        verify(manager).setAdapter(list.adapter)
    }

    @Test
    fun `GIVEN a SILLM WHEN onSaveInstanceState called THEN it returns a new SavedState with the scroll data`() {
        manager.scrollPosition = 42
        manager.scrollOffset = 422

        val result: SavedState = manager.onSaveInstanceState() as SavedState

        assertTrue(result.superState is LinearLayoutManager.SavedState)
        assertEquals(42, result.scrollPosition)
        assertEquals(422, result.scrollOffset)
    }

    @Test
    fun `GIVEN a SILLM WHEN onRestoreInstanceState is called with a new state THEN it updates scrollPosition and scrollOffset`() {
        val newState = SavedState(
            null,
            scrollPosition = 222,
            scrollOffset = 221,
        )

        manager.onRestoreInstanceState(newState)

        assertEquals(222, manager.scrollPosition)
        assertEquals(221, manager.scrollOffset)
    }

    @Test
    fun `GIVEN a SILLM WHEN onRestoreInstanceState is called with a null state THEN scrollPosition and scrollOffset are left unchanged`() {
        manager.onRestoreInstanceState(null)

        assertEquals(RecyclerView.NO_POSITION, manager.scrollPosition)
        assertEquals(0, manager.scrollOffset)
    }

    @Test
    fun `GIVEN a SILLM WHEN onLayoutChildren is called while in preLayout THEN it execute the super with the sticky item detached and not updates stickyItem`() {
        manager = spy(manager)
        val listState: RecyclerView.State = mock()
        doReturn(true).`when`(listState).isPreLayout

        manager.onLayoutChildren(mock(), listState)

        verify(manager).restoreView<Unit>(any())
        verify(manager, never()).updateStickyItem(any(), ArgumentMatchers.anyBoolean())
    }

    @Test
    fun `GIVEN a SILLM WHEN onLayoutChildren is called while not in preLayout THEN it execute the super with the sticky item detached and updates stickyItem`() {
        manager = spy(manager)
        val recycler: RecyclerView.Recycler = mock()
        val listState: RecyclerView.State = mock()
        doReturn(false).`when`(listState).isPreLayout
        // Prevent side effects following the "manager.onLayoutChildren" call
        doReturn(false).`when`(manager).shouldStickyItemBeShownForCurrentPosition()

        manager.onLayoutChildren(recycler, listState)

        verify(manager).restoreView<Unit>(any())
        verify(manager).updateStickyItem(recycler, true)
    }

    @Test
    fun `GIVEN A SILLM WHEN scrollVerticallyBy is called THEN it detaches the sticky item to scroll using parent and not updates the sticky item`() {
        manager = spy(manager)

        val result = manager.scrollVerticallyBy(0, mock(), mock())

        verify(manager).restoreView<Int>(any())
        verify(manager, never()).updateStickyItem(any(), ArgumentMatchers.anyBoolean())
        assertEquals(0, result)
    }

    @Test
    fun `GIVEN a SILLM WHEN findLastVisibleItemPosition is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.findLastVisibleItemPosition()

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN findFirstVisibleItemPosition is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.findFirstVisibleItemPosition()

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN findFirstCompletelyVisibleItemPosition is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.findFirstCompletelyVisibleItemPosition()

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN findLastCompletelyVisibleItemPosition is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.findLastCompletelyVisibleItemPosition()

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN computeVerticalScrollExtent is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.computeVerticalScrollExtent(mock())

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN computeVerticalScrollOffset is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.computeVerticalScrollOffset(mock())

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN computeVerticalScrollRange is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.computeVerticalScrollRange(mock())

        verify(manager).restoreView<Int>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN computeScrollVectorForPosition is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.computeScrollVectorForPosition(33)

        verify(manager).restoreView<PointF>(any())
    }

    @Test
    fun `GIVEN sticky item is null WHEN scrollToPosition is called THEN scrollToPositionWithOffset is not called`() {
        manager = spy(manager)

        manager.scrollToPosition(32)

        verify(manager, never()).scrollToPositionWithOffset(ArgumentMatchers.anyInt(), ArgumentMatchers.anyInt())
    }

    @Test
    fun `GIVEN sticky item is not null WHEN scrollToPosition is called THEN it calls scrollToPositionWithOffset with INVALID_OFFSET`() {
        manager = spy(manager)
        manager.stickyItemView = mock()

        manager.scrollToPosition(32)

        verify(manager).scrollToPositionWithOffset(32, INVALID_OFFSET)
    }

    @Test
    fun `GIVEN sticky item is not null WHEN scrollToPositionWithOffset is called THEN scrollToIndicatedPositionWithOffset is delegated`() {
        manager = spy(manager)
        manager.stickyItemView = mock()

        manager.scrollToPositionWithOffset(23, 9)

        verify(manager).setScrollState(RecyclerView.NO_POSITION, INVALID_OFFSET)
        verify(manager).scrollToIndicatedPositionWithOffset(eq(23), eq(9), any())
        verify(manager).setScrollState(23, 9)
    }

    @Test
    fun `GIVEN sticky item is null WHEN onFocusSearchFailed is called THEN it detaches the sticky item to call the super method`() {
        manager = spy(manager)

        manager.onFocusSearchFailed(mock(), 3, mock(), mock())

        verify(manager).restoreView<View?>(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN getAdapterPositionForItemIndex is called with a index for which there is no bound view THEN it returns -1`() {
        assertEquals(RecyclerView.NO_POSITION, manager.getAdapterPositionForItemIndex(22))
    }

    @Test
    fun `GIVEN a SILLM WHEN getAdapterPositionForItemIndex is called with a index of an existing view THEN it returns it's absoluteAdapterPosition`() {
        manager = spy(manager)
        val params: RecyclerView.LayoutParams = mock()
        doReturn(7).`when`(params).absoluteAdapterPosition
        val view: View = mock()
        doReturn(params).`when`(view).layoutParams
        doReturn(view).`when`(manager).getChildAt(ArgumentMatchers.anyInt())

        assertEquals(7, manager.getAdapterPositionForItemIndex(22))
    }

    @Test
    fun `GIVEN a SILLM WHEN setAdapter is called with a null argument THEN the current adapter and stickyItem are set to null`() {
        val initialAdapter = mock<FakeStickyItemsAdapter>()
        manager.listAdapter = initialAdapter

        manager.setAdapter(null)

        verify(initialAdapter).unregisterAdapterDataObserver(manager.stickyItemPositionsObserver)
        assertNull(manager.listAdapter)
        assertNull(manager.stickyItemView)
    }

    @Test
    fun `GIVEN a SILLM WHEN setAdapter is called with a new valid adapter THEN the current adapter is reset`() {
        val initialAdapter: FakeStickyItemsAdapter = mock()
        val newAdapter: FakeStickyItemsAdapter = mock()
        manager.listAdapter = initialAdapter
        manager.stickyItemPositionsObserver = spy(manager.stickyItemPositionsObserver)

        manager.setAdapter(newAdapter)

        verify(initialAdapter).unregisterAdapterDataObserver(manager.stickyItemPositionsObserver)
        assertSame(newAdapter, manager.listAdapter)
        verify(newAdapter).registerAdapterDataObserver(manager.stickyItemPositionsObserver)
        verify(manager.stickyItemPositionsObserver).onChanged()
    }

    @Test
    fun `GIVEN a SILLM WHEN restoreView is called with a method parameter THEN the sticky item is detached, method executed, item reattached`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        doNothing().`when`(manager).detachView(any())
        doNothing().`when`(manager).attachView(any())
        val orderVerifier = Mockito.inOrder(manager)

        val result = manager.restoreView { 3 }

        orderVerifier.verify(manager).detachView(stickyView)
        orderVerifier.verify(manager).attachView(stickyView)
        assertEquals(3, result)
    }

    @Test
    fun `GIVEN sticky item should not be shown WHEN updateStickyItem is called THEN the stickyItemView is recycled`() {
        manager = spy(manager)
        manager.stickyItemView = mock()
        doReturn(false).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doNothing().`when`(manager).recycleStickyItem(any())
        val recycler: RecyclerView.Recycler = mock()

        manager.updateStickyItem(recycler, true)

        verify(manager).recycleStickyItem(recycler)
    }

    @Test
    fun `GIVEN sticky item should be shown and not exists WHEN updateStickyItem is called THEN a new stickyItemView is created`() {
        manager = spy(manager)
        doReturn(true).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doReturn(0f).`when`(manager).getY(any())
        manager.stickyItemPosition = 42
        val recycler: RecyclerView.Recycler = mock()
        doNothing().`when`(manager).createStickyView(any(), ArgumentMatchers.anyInt())

        manager.updateStickyItem(recycler, false)

        verify(manager).createStickyView(recycler, 42)
        verify(manager, never()).recycleStickyItem(any())
    }

    @Test
    fun `GIVEN sticky item should be shown and exists WHEN updateStickyItem is called THEN another stickyItemView is not created`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        doReturn(true).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doReturn(0f).`when`(manager).getY(any())
        manager.stickyItemPosition = 42
        val recycler: RecyclerView.Recycler = mock()
        doNothing().`when`(manager).createStickyView(any(), ArgumentMatchers.anyInt())

        manager.updateStickyItem(recycler, false)

        verify(manager, never()).createStickyView(any(), ArgumentMatchers.anyInt())
        verify(manager, never()).recycleStickyItem(any())
    }

    @Test
    fun `GIVEN sticky item should be shown WHEN updateStickyItem is called while layout THEN bindStickyItem is called`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        doReturn(true).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doReturn(0f).`when`(manager).getY(any())
        doNothing().`when`(manager).createStickyView(any(), ArgumentMatchers.anyInt())
        doNothing().`when`(manager).bindStickyItem(any())

        manager.updateStickyItem(mock(), true)

        verify(manager).bindStickyItem(stickyView)
        verify(manager, never()).recycleStickyItem(any())
    }

    @Test
    fun `GIVEN sticky item should be shown WHEN updateStickyItem is called while not layout THEN bindStickyItem is not called`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        doReturn(true).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doReturn(0f).`when`(manager).getY(any())
        doNothing().`when`(manager).createStickyView(any(), ArgumentMatchers.anyInt())
        doNothing().`when`(manager).bindStickyItem(any())

        manager.updateStickyItem(mock(), false)

        verify(manager, never()).bindStickyItem(any())
        verify(manager, never()).recycleStickyItem(any())
    }

    @Test
    fun `GIVEN sticky item should be shown and it's view exists WHEN updateStickyItem is called THEN the stickyItemView gets set a new Y translation`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        doReturn(true).`when`(manager).shouldStickyItemBeShownForCurrentPosition()
        doReturn(44f).`when`(manager).getY(any())
        doNothing().`when`(manager).createStickyView(any(), ArgumentMatchers.anyInt())

        manager.updateStickyItem(mock(), false)

        verify(manager).getY(stickyView)
        verify(stickyView).translationY = 44f
        verify(manager, never()).recycleStickyItem(any())
    }

    @Test
    fun `GIVEN SILLM WHEN createStickyView is called THEN a new View is created and cached in stickyItemView`() {
        manager = spy(manager)
        val adapter: FakeStickyItemsAdapter = mock()
        manager.listAdapter = adapter
        val recycler: RecyclerView.Recycler = mock()
        val newStickyView: View = mock()
        doReturn(newStickyView).`when`(recycler).getViewForPosition(ArgumentMatchers.anyInt())
        doNothing().`when`(manager).addView(any())
        doNothing().`when`(manager).measureAndLayout(any())
        doNothing().`when`(manager).ignoreView(any())

        manager.createStickyView(recycler, 22)

        verify(adapter).setupStickyItem(newStickyView)
        verify(manager).addView(newStickyView)
        verify(manager).measureAndLayout(newStickyView)
        verify(manager).ignoreView(newStickyView)
        assertSame(newStickyView, manager.stickyItemView)
    }

    @Test
    fun `GIVEN a SILLM WHEN bindStickyItem is called for a new View THEN the view is measured and layout`() {
        manager = spy(manager)
        val view: View = mock()
        doNothing().`when`(manager).measureAndLayout(any())

        manager.bindStickyItem(view)

        verify(manager).measureAndLayout(view)
    }

    @Test
    fun `GIVEN a pending scroll WHEN bindStickyItem is called for a new View THEN a OnGlobalLayoutListener is set`() {
        manager = spy(manager)
        manager.scrollPosition = 22
        val view: View = mock()
        val viewObserver: ViewTreeObserver = mock()
        doReturn(viewObserver).`when`(view).viewTreeObserver
        doNothing().`when`(manager).measureAndLayout(any())

        manager.bindStickyItem(view)

        verify(manager).measureAndLayout(view)
        verify(viewObserver).addOnGlobalLayoutListener(any())
    }

    @Test
    fun `GIVEN no pending scroll WHEN bindStickyItem is called for a new View THEN no OnGlobalLayoutListener is set`() {
        manager = spy(manager)
        val view: View = mock()
        val viewObserver: ViewTreeObserver = mock()
        doReturn(viewObserver).`when`(view).viewTreeObserver
        doNothing().`when`(manager).measureAndLayout(any())

        manager.bindStickyItem(view)

        verify(manager).measureAndLayout(view)
        verify(viewObserver, never()).addOnGlobalLayoutListener(any())
    }

    @Test
    fun `GIVEN a SILLM WHEN measureAndLayout is called for a new View THEN it is measured and layout`() {
        manager = spy(manager)
        val newView: View = mock()
        doReturn(22).`when`(manager).paddingLeft
        doReturn(33).`when`(manager).paddingRight
        doReturn(100).`when`(manager).width
        doReturn(112).`when`(newView).measuredHeight

        doNothing().`when`(manager).measureChildWithMargins(any(), ArgumentMatchers.anyInt(), ArgumentMatchers.anyInt())

        manager.measureAndLayout(newView)

        verify(manager).measureChildWithMargins(newView, 0, 0)
        verify(newView).layout(22, 0, (100 - 33), 112)
    }

    @Test
    fun `GIVEN a SILLM WHEN recycleStickyItem is called THEN the view holder is reset and allowed to be recycled`() {
        manager = spy(manager)
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        val adapter: FakeStickyItemsAdapter = mock()
        manager.listAdapter = adapter
        val recycler: RecyclerView.Recycler = mock()
        val captor = argumentCaptor<View>()
        doNothing().`when`(manager).stopIgnoringView(any())
        doNothing().`when`(manager).removeView(any())

        manager.recycleStickyItem(recycler)

        verify(adapter).tearDownStickyItem(captor.capture())
        verify(manager).stopIgnoringView(captor.value)
        verify(manager).removeView(captor.value)
        verify(recycler).recycleView(captor.value)
    }

    @Test
    fun `GIVEN a SILLM WHEN is called with a new position and offset THEN they are cached in scrollPosition and scrollOffset properties`() {
        manager.setScrollState(222, 333)

        assertEquals(222, manager.scrollPosition)
        assertEquals(333, manager.scrollOffset)
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN onChanged is called THEN handleChange() is delegated`() {
        val observer = spy(manager.stickyItemPositionsObserver)
        manager.stickyItemPositionsObserver = observer

        observer.onChanged()

        verify(observer).handleChange()
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN onItemRangeInserted is called THEN handleChange() is delegated`() {
        val observer = spy(manager.stickyItemPositionsObserver)
        manager.stickyItemPositionsObserver = observer

        observer.onItemRangeInserted(22, 33)

        verify(observer).handleChange()
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN onItemRangeRemoved is called THEN handleChange() is delegated`() {
        val observer = spy(manager.stickyItemPositionsObserver)
        manager.stickyItemPositionsObserver = observer

        observer.onItemRangeRemoved(22, 33)

        verify(observer).handleChange()
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN onItemRangeMoved is called THEN handleChange() is delegated`() {
        val observer = spy(manager.stickyItemPositionsObserver)
        manager.stickyItemPositionsObserver = observer

        observer.onItemRangeMoved(11, 22, 33)

        verify(observer).handleChange()
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN handleChange is called THEN the sticky item is updated`() {
        manager = spy(manager)
        val adapter: FakeStickyItemsAdapter = mock()
        manager.listAdapter = adapter
        val stickyView: View = mock()
        manager.stickyItemView = stickyView
        val observer = spy(manager.ItemPositionsAdapterDataObserver())
        manager.stickyItemPositionsObserver = observer
        doReturn(23).`when`(observer).calculateNewStickyItemPosition(any())
        doNothing().`when`(manager).recycleStickyItem(any())

        observer.handleChange()

        verify(observer).calculateNewStickyItemPosition(adapter)
        verify(manager).recycleStickyItem(null)
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN calculateNewStickyItemPosition is called for a top item the sticky position is first in adaptor`() {
        manager = spy(FakeStickyItemLayoutManager(mock(), StickyItemPlacement.TOP))
        val adapter: FakeStickyItemsAdapter = mock()
        doReturn(true).`when`(adapter).isStickyItem(3)
        doReturn(true).`when`(adapter).isStickyItem(5)
        doReturn(10).`when`(manager).itemCount
        manager.stickyItemPositionsObserver = manager.ItemPositionsAdapterDataObserver()

        assertEquals(3, manager.stickyItemPositionsObserver.calculateNewStickyItemPosition(adapter))
    }

    @Test
    fun `GIVEN an ItemPositionsAdapterDataObserver WHEN calculateNewStickyItemPosition is called for a bottom item the sticky position is last in adaptor`() {
        manager = spy(FakeStickyItemLayoutManager(mock(), StickyItemPlacement.BOTTOM))
        val adapter: FakeStickyItemsAdapter = mock()
        doReturn(true).`when`(adapter).isStickyItem(3)
        doReturn(true).`when`(adapter).isStickyItem(5)
        doReturn(10).`when`(manager).itemCount
        manager.stickyItemPositionsObserver = manager.ItemPositionsAdapterDataObserver()

        assertEquals(5, manager.stickyItemPositionsObserver.calculateNewStickyItemPosition(adapter))
    }

    @Test
    fun `WHEN get is called for a reversed StickyItemPlacement#TOP layout manager THEN a StickyHeaderLinearLayoutManager is returned`() {
        val result = StickyItemsLinearLayoutManager.get<FakeStickyItemsAdapter>(
            mock(),
            StickyItemPlacement.TOP,
            true,
        )

        assertTrue(result is StickyHeaderLinearLayoutManager)
        assertTrue(result.reverseLayout)
    }

    @Test
    fun `WHEN get is called for a not reversed StickyItemPlacement#TOP layout manager THEN a StickyHeaderLinearLayoutManager is returned`() {
        val result = StickyItemsLinearLayoutManager.get<FakeStickyItemsAdapter>(
            mock(),
            StickyItemPlacement.TOP,
            false,
        )

        assertTrue(result is StickyHeaderLinearLayoutManager)
        assertFalse(result.reverseLayout)
    }

    @Test
    fun `WHEN get is called for a reversed StickyItemPlacement#BOTTOM layout manager THEN a StickyFooterLinearLayoutManager is returned`() {
        val result = StickyItemsLinearLayoutManager.get<FakeStickyItemsAdapter>(
            mock(),
            StickyItemPlacement.BOTTOM,
            true,
        )

        assertTrue(result is StickyFooterLinearLayoutManager)
        assertTrue(result.reverseLayout)
    }

    @Test
    fun `WHEN get is called for a not reversed StickyItemPlacement#BOTTOM layout manager THEN a StickyFooterLinearLayoutManager is returned`() {
        val result = StickyItemsLinearLayoutManager.get<FakeStickyItemsAdapter>(
            mock(),
            StickyItemPlacement.BOTTOM,
            false,
        )

        assertTrue(result is StickyFooterLinearLayoutManager)
        assertFalse(result.reverseLayout)
    }
}
