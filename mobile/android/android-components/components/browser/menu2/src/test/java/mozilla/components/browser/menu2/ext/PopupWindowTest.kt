/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.graphics.Rect
import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.core.view.ViewCompat
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.adapter.MenuCandidateListAdapter
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import kotlin.math.roundToInt

/**
 * [PopupWindow] const's.
 */
private const val HALF_MENU_ITEM = 0.5

/**
 * [PopupWindow] UI components.
 */
private const val ANCHOR_ROOT_VIEW_HEIGHT = 1000
private const val ANCHOR_ROOT_VIEW_WIDTH = 400
private const val ITEM_HEIGHT = 50
private const val CONTAINER_PADDING = 10

@RunWith(AndroidJUnit4::class)
class PopupWindowTest {

    private lateinit var containerView: View
    private lateinit var anchor: View
    private lateinit var popupWindow: PopupWindow

    @Before
    fun setUp() {
        containerView = mock(View::class.java)
        doReturn(createContainerRecyclerView()).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        popupWindow = spy(PopupWindow())
    }

    @Test
    fun `WHEN recycler view has no adapter THEN show is never called`() {
        val containerRecyclerView = createContainerRecyclerView(hasAdapter = false)
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        anchor = mock(View::class.java)

        assertFalse(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        verify(popupWindow, never()).showAsDropDown(any())
        verify(popupWindow, never()).showAsDropDown(any(), anyInt(), anyInt())
        verify(popupWindow, never()).showAtLocation(any(), anyInt(), anyInt(), anyInt())
    }

    @Test
    fun `WHEN container view has no measured height THEN show is never called`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)
        `when`(containerView.measuredHeight).thenReturn(0)

        anchor = mock(View::class.java)

        assertFalse(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        verify(popupWindow, never()).showAsDropDown(any())
        verify(popupWindow, never()).showAsDropDown(any(), anyInt(), anyInt())
        verify(popupWindow, never()).showAtLocation(any(), anyInt(), anyInt(), anyInt())
    }

    @Test
    fun `WHEN recycler view has no measured height THEN show is never called`() {
        val containerRecyclerView = createContainerRecyclerView(recyclerViewHeight = 0)
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = mock(View::class.java)

        assertFalse(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        verify(popupWindow, never()).showAsDropDown(any())
        verify(popupWindow, never()).showAsDropDown(any(), anyInt(), anyInt())
        verify(popupWindow, never()).showAtLocation(any(), anyInt(), anyInt(), anyInt())
    }

    @Test
    fun `WHEN recycler view has no items THEN show is never called`() {
        val containerRecyclerView =
            createContainerRecyclerView(recyclerViewHeight = 100, itemCount = 0)
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = mock(View::class.java)

        assertFalse(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        verify(popupWindow, never()).showAsDropDown(any())
        verify(popupWindow, never()).showAsDropDown(any(), anyInt(), anyInt())
        verify(popupWindow, never()).showAtLocation(any(), anyInt(), anyInt(), anyInt())
    }

    @Test
    fun `WHEN orientation up & fits available height THEN showAsDropDown with original height & positioned from the bottom of the anchor with leftBottomAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = createAnchor(20, ANCHOR_ROOT_VIEW_HEIGHT - 20)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAsDropDown(anchor, 0, -containerHeight)
    }

    @Test
    fun `WHEN orientation up & fits available height & is RTL THEN showAsDropDown with original height & positioned from the bottom of the anchor with rightBottomAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = createAnchor(ANCHOR_ROOT_VIEW_WIDTH - 20, ANCHOR_ROOT_VIEW_HEIGHT - 20, true)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAsDropDown(anchor, 0, -containerHeight)
    }

    @Test
    fun `WHEN orientation up & does not fit available height THEN showAtLocation with original height & positioned from the bottom of the anchor with leftAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        val (x, y) = 20 to 25
        anchor = createAnchor(x, y)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, x, y)
    }

    @Test
    fun `WHEN orientation up & does not fit available height & is RTL THEN showAtLocation with original height & positioned from the bottom of the anchor with rightAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        val (x, y) = ANCHOR_ROOT_VIEW_HEIGHT - 20 to 25
        anchor = createAnchor(x, y, true)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, x, y)
    }

    @Test
    fun `WHEN orientation down & fits available height THEN showAsDropDown with original height & positioned from the top of the anchor with leftTopAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = createAnchor(20, 25)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.DOWN))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAsDropDown(anchor)
    }

    @Test
    fun `WHEN orientation down & fits available height & is RTL THEN showAsDropDown with original height & positioned from the top of the anchor with rightTopAnimation`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = createAnchor(ANCHOR_ROOT_VIEW_WIDTH - 20, 25, true)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.DOWN))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAsDropDown(anchor)
    }

    @Test
    fun `WHEN number of menu items don't fit available height THEN set popup with calculated height`() {
        val containerRecyclerView = createContainerRecyclerView(itemCount = 22)
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        anchor = createAnchor(20, 25)

        assertTrue(popupWindow.displayPopup(containerView, anchor, Orientation.UP))

        val availableHeight = anchor.rootView.measuredHeight
        val maxAvailableHeightForRecyclerView = availableHeight - CONTAINER_PADDING
        val numberOfItemsFitExactly =
            (maxAvailableHeightForRecyclerView.toFloat() / ITEM_HEIGHT.toFloat()).roundToInt()
        val numberOfItemsFitWithOverFlow = numberOfItemsFitExactly - HALF_MENU_ITEM
        val updatedRecyclerViewHeight = (numberOfItemsFitWithOverFlow * ITEM_HEIGHT).toInt()
        val calculatedHeight = updatedRecyclerViewHeight + CONTAINER_PADDING

        assertEquals(calculatedHeight, popupWindow.height)
    }

    @Test
    fun `WHEN orientation is null THEN showAtAnchorLocation is called with the anchor info & default gravity`() {
        val containerRecyclerView = createContainerRecyclerView()
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        val (x, y) = 20 to 25
        anchor = createAnchor(x, y)

        assertTrue(popupWindow.displayPopup(containerView, anchor))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, x, y)
    }

    @Test
    fun `WHEN orientation is null & is RTL THEN showAtAnchorLocation is called with the anchor info & default gravity`() {
        val containerRecyclerView = createContainerRecyclerView(itemCount = 20)
        doReturn(containerRecyclerView).`when`(containerView)
            .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)

        val containerHeight = containerRecyclerView.measuredHeight.plus(CONTAINER_PADDING)
        `when`(containerView.measuredHeight).thenReturn(containerHeight)

        val (x, y) = ANCHOR_ROOT_VIEW_WIDTH - 20 to 25
        anchor = createAnchor(x, y, true)

        assertTrue(popupWindow.displayPopup(containerView, anchor))

        assertEquals(containerHeight, popupWindow.height)
        assertEquals(
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight,
            popupWindow.animationStyle,
        )
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, x, y)
    }

    private fun createContainerRecyclerView(
        itemCount: Int = 10,
        hasAdapter: Boolean = true,
        recyclerViewHeight: Int? = null,
    ): RecyclerView {
        val recyclerView = mock(RecyclerView::class.java)

        if (hasAdapter) {
            val adapter = mock(MenuCandidateListAdapter::class.java)
            `when`(adapter.itemCount).thenReturn(itemCount)
            `when`(recyclerView.adapter).thenReturn(adapter)
        }

        `when`(recyclerView.measuredHeight).thenReturn(
            recyclerViewHeight
                ?: (itemCount * ITEM_HEIGHT),
        )

        return recyclerView
    }

    private fun createAnchor(x: Int, y: Int, isRTL: Boolean = false): View {
        val view = spy(View(testContext))

        doAnswer { invocation ->
            val locationOnScreen = (invocation.getArgument(0) as IntArray)
            locationOnScreen[0] = x
            locationOnScreen[1] = y
            locationOnScreen
        }.`when`(view).getLocationOnScreen(any())

        doAnswer { invocation ->
            val locationInWindow = (invocation.getArgument(0) as IntArray)
            locationInWindow[0] = x
            locationInWindow[1] = y
            locationInWindow
        }.`when`(view).getLocationInWindow(any())

        if (isRTL) {
            doReturn(ViewCompat.LAYOUT_DIRECTION_RTL).`when`(view).layoutDirection
        } else {
            doReturn(ViewCompat.LAYOUT_DIRECTION_LTR).`when`(view).layoutDirection
        }
        doReturn(10).`when`(view).height
        doReturn(15).`when`(view).width

        val anchorRootView = createAnchorRootView()
        `when`(view.rootView).thenReturn(anchorRootView)

        return view
    }

    private fun createAnchorRootView(): View {
        val view = spy(View(testContext))
        doAnswer { invocation ->
            val displayFrame = (invocation.getArgument(0) as Rect)
            displayFrame.left = 0
            displayFrame.right = ANCHOR_ROOT_VIEW_WIDTH
            displayFrame
        }.`when`(view).getWindowVisibleDisplayFrame(any())

        `when`(view.measuredHeight).thenReturn(ANCHOR_ROOT_VIEW_HEIGHT)

        return view
    }
}
