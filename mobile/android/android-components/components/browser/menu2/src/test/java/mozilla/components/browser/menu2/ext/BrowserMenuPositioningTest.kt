/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.graphics.Rect
import android.view.View
import android.widget.PopupWindow
import androidx.core.view.ViewCompat
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.adapter.MenuCandidateListAdapter
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`
import kotlin.math.roundToInt

/**
 * [PopupWindow] const's.
 */
private const val HALF_MENU_ITEM = 0.5

/**
 * [PopupWindow] UI components.
 */
private const val SCREEN_ROOT_VIEW_HEIGHT = 1000
private const val SCREEN_ROOT_VIEW_WIDTH = 400
private const val MENU_ITEM_HEIGHT = 50
private const val DEFAULT_ITEM_COUNT = 10
private const val MENU_CONTAINER_WIDTH = 100
private const val MENU_CONTAINER_PADDING = 10

@RunWith(AndroidJUnit4::class)
class BrowserMenuPositioningTest {
    private lateinit var popupWindow: PopupWindow
    private lateinit var overlapStyle: MenuStyle
    private lateinit var offsetStyle: MenuStyle
    private lateinit var offsetOverlapStyle: MenuStyle

    @Before
    fun setUp() {
        overlapStyle = MenuStyle(completelyOverlap = true)
        offsetStyle = MenuStyle(horizontalOffset = 10, verticalOffset = 10)
        offsetOverlapStyle =
            MenuStyle(completelyOverlap = true, horizontalOffset = 10, verticalOffset = 10)

        popupWindow = spy(PopupWindow())
    }

    @Test
    fun `WHEN recycler view has no adapter THEN menu positioning data is null`() {
        val containerView = createContainerView(hasAdapter = false)
        val anchor = mock(View::class.java)

        assertNull(inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP))
    }

    @Test
    fun `WHEN container view has no measured height THEN menu positioning data is null`() {
        val containerView = createContainerView()
        val anchor = mock(View::class.java)

        `when`(containerView.measuredHeight).thenReturn(0)

        assertNull(inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP))
    }

    @Test
    fun `WHEN container view has no measured width THEN menu positioning data is null`() {
        val containerView = createContainerView()
        val anchor = mock(View::class.java)

        `when`(containerView.measuredWidth).thenReturn(0)

        assertNull(inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP))
    }

    @Test
    fun `WHEN recycler view has no measured height THEN menu positioning data is null`() {
        val containerView = createContainerView(recyclerViewHeight = 0)
        val anchor = mock(View::class.java)

        assertNull(inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP))
    }

    @Test
    fun `WHEN recycler view has no items THEN menu positioning data is null`() {
        val containerView = createContainerView(recyclerViewHeight = 100, itemCount = 0)
        val anchor = mock(View::class.java)

        assertNull(inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP))
    }

    @Test
    fun `WHEN orientation up & fits available height THEN original height & positioned from the bottom of the anchor with leftBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation up & fits available height & is RTL THEN original height & positioned from the bottom right of the anchor with rightBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation up & does not fit available height & fits down THEN original height & positioned from the top left of the anchor with leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation up & does not fit up or down THEN original height & positioned from the top left of the anchor with leftAnimation`() {
        val containerView = createContainerView()

        val notEnoughHeight = containerView.measuredHeight.minus(MENU_CONTAINER_PADDING).minus(1)
        val (x, y) = 20 to notEnoughHeight
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation up & does not fit up or down & is RTL THEN original height & positioned from the bottom right of the anchor with rightAnimation`() {
        val containerView = createContainerView()

        val notEnoughHeight = containerView.measuredHeight.minus(MENU_CONTAINER_PADDING).minus(1)
        val (x, y) = SCREEN_ROOT_VIEW_HEIGHT - 20 to notEnoughHeight
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionRight = false, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation down & fits available height THEN original height & positioned from the top left of the anchor with leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation down & fits available height & is RTL THEN original height & positioned from the top right of the anchor with rightTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to 25
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, orientation = Orientation.DOWN)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionRight = false, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN number of menu items don't fit available height THEN set popup with calculated height`() {
        val containerView = createContainerView(itemCount = 22)
        val anchor = createAnchor(20, 25)

        val positioningData = inferMenuPositioningData(containerView, anchor, orientation = Orientation.UP)

        val availableHeight = anchor.rootView.measuredHeight
        val maxAvailableHeightForRecyclerView = availableHeight - MENU_CONTAINER_PADDING
        val numberOfItemsFitExactly =
            (maxAvailableHeightForRecyclerView.toFloat() / MENU_ITEM_HEIGHT.toFloat()).roundToInt()
        val numberOfItemsFitWithOverFlow = numberOfItemsFitExactly - HALF_MENU_ITEM
        val updatedRecyclerViewHeight = (numberOfItemsFitWithOverFlow * MENU_ITEM_HEIGHT).toInt()
        val calculatedHeight = updatedRecyclerViewHeight + MENU_CONTAINER_PADDING

        assertEquals(calculatedHeight, positioningData?.containerHeight)
    }

    @Test
    fun `WHEN orientation is null & menu fits down THEN original height & positioned from the top left of the anchor with leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, orientation = null)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN orientation is null & menu does not fit & is RTL THEN showAtLocation with original height & positioned from the top right of the anchor with rightAnimation`() {
        val containerView = createContainerView(itemCount = 20)

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to 25
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, orientation = null)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, directionRight = false, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation up & fits up THEN original height & bottom of the menu positioned exactly to the bottom of the anchor with leftBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation up & fits up & is RTL THEN original height & bottom right of the menu positioned exactly to the bottom right of the anchor with rightBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation down & fits down THEN showAtLocation with original height & positioned exactly from the top of the anchor with leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.DOWN)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation down & fits down & is RTL THEN original height & positioned exactly from the top of the anchor with rightTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to 25
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.DOWN)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle, directionUp = false, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation up & does not fit up & fits down THEN original height & positioned exactly from the top left of the anchor with leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should completely overlap & orientation up & does not fit up or down THEN original height & positioned exactly from the top left of the anchor with leftAnimation`() {
        val containerView = createContainerView()

        val notEnoughHeight = containerView.measuredHeight.minus(MENU_CONTAINER_PADDING).minus(1)
        val (x, y) = 20 to notEnoughHeight
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, overlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, overlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation up & fits up THEN original height & positioned from the bottom left of the anchor with applied offsets and leftBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation up & fits up & is RTL THEN original height & positioned from the bottom right of the anchor with applied offsets and rightBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation down & fits down THEN original height & positioned from the top left of the anchor with applied offsets and leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation down & fits down & is RTL THEN original height & positioned from the top right of the anchor with applied offsets and rightTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to 25
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle, directionUp = false, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation up & does not fit up & fits down THEN original height & positioned from the top left of the anchor with applied offsets and leftAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN has style offsets & orientation up & does not fit up or down THEN original height & positioned from the top left of the anchor with applied offsets leftAnimation`() {
        val containerView = createContainerView()

        val notEnoughHeight = containerView.measuredHeight.minus(MENU_CONTAINER_PADDING).minus(1)
        val (x, y) = 20 to notEnoughHeight
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation up & fits up THEN original height & positioned exactly from the bottom left of the anchor with applied offsets leftBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation up & fits up & is RTL THEN original height & positioned exactly to the bottom right of the anchor with applied offsets rightBottomAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to SCREEN_ROOT_VIEW_HEIGHT - 20
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation down & fits down THEN original height & positioned exactly from the top of the anchor with applied offsets leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation down & fits down & is RTL THEN original height & positioned exactly from the top of the anchor with applied offsets and rightTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = SCREEN_ROOT_VIEW_WIDTH - 20 to 25
        val anchor = createAnchor(x, y, true)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle, directionUp = false, directionRight = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation up & fits down only THEN original height & positioned exactly from the top left of the anchor with applied offsets and leftTopAnimation`() {
        val containerView = createContainerView()

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN should overlap & has style offsets & orientation up & does not fit up or down THEN original height & positioned exactly from the top left of the anchor with applied offsets and leftAnimation`() {
        val containerView = createContainerView()

        val notEnoughHeight = containerView.measuredHeight.minus(MENU_CONTAINER_PADDING).minus(1)
        val (x, y) = 20 to notEnoughHeight
        val anchor = createAnchor(x, y)

        val result = inferMenuPositioningData(containerView, anchor, offsetOverlapStyle, Orientation.UP)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor, offsetOverlapStyle, directionUp = false)
        val expected = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft,
        )

        assertEquals(expected, result)
    }
}

internal fun createContainerView(
    itemCount: Int = DEFAULT_ITEM_COUNT,
    hasAdapter: Boolean = true,
    recyclerViewHeight: Int? = null,
): View {
    val containerView = mock(View::class.java)

    // Mimicking elevation added spacing.
    val recyclerView = createRecyclerView(itemCount, hasAdapter, recyclerViewHeight)
    val containerHeight = recyclerView.measuredHeight.plus(MENU_CONTAINER_PADDING)
    val containerWidth = recyclerView.measuredWidth.plus(MENU_CONTAINER_PADDING)
    `when`(containerView.measuredHeight).thenReturn(containerHeight)
    `when`(containerView.measuredWidth).thenReturn(containerWidth)

    doReturn(recyclerView).`when`(containerView)
        .findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)
    return containerView
}

private fun createRecyclerView(
    itemCount: Int = DEFAULT_ITEM_COUNT,
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
            ?: (itemCount * MENU_ITEM_HEIGHT),
    )

    `when`(recyclerView.measuredWidth).thenReturn(MENU_CONTAINER_WIDTH)

    return recyclerView
}

internal fun createAnchor(x: Int, y: Int, isRTL: Boolean = false): View {
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
        displayFrame.right = SCREEN_ROOT_VIEW_WIDTH
        displayFrame.bottom = SCREEN_ROOT_VIEW_HEIGHT
        displayFrame
    }.`when`(view).getWindowVisibleDisplayFrame(any())

    `when`(view.measuredHeight).thenReturn(SCREEN_ROOT_VIEW_HEIGHT)

    return view
}

internal fun getTargetCoordinates(
    anchorX: Int,
    anchorY: Int,
    containerView: View,
    anchor: View,
    style: MenuStyle? = null,
    directionUp: Boolean = true,
    directionRight: Boolean = true,
): Pair<Int, Int> {
    val targetX = getTargetX(
        anchorX,
        containerView,
        anchor,
        directionRight,
        style?.completelyOverlap ?: false,
        style?.horizontalOffset ?: 0,
    )
    val targetY = getTargetY(
        anchorY,
        containerView,
        anchor,
        directionUp,
        style?.completelyOverlap ?: false,
        style?.verticalOffset ?: 0,
    )
    return targetX to targetY
}

private fun getTargetX(
    anchorX: Int,
    containerView: View,
    anchor: View,
    directionRight: Boolean,
    shouldOverlap: Boolean,
    horizontalOffset: Int,
): Int {
    val targetX = when {
        directionRight && shouldOverlap -> anchorX - (MENU_CONTAINER_PADDING / 2)
        directionRight && !shouldOverlap -> anchorX
        !directionRight && shouldOverlap -> anchorX - (containerView.measuredWidth - anchor.width) + (MENU_CONTAINER_PADDING / 2)
        else -> anchorX - (containerView.measuredWidth - anchor.width)
    }

    return if (directionRight) {
        targetX + horizontalOffset
    } else {
        targetX - horizontalOffset
    }
}

private fun getTargetY(
    anchorY: Int,
    containerView: View,
    anchor: View,
    directionUp: Boolean,
    shouldOverlap: Boolean,
    verticalOffset: Int,
): Int {
    val targetY = when {
        directionUp && shouldOverlap -> anchorY - (containerView.measuredHeight - anchor.height) + (MENU_CONTAINER_PADDING / 2)
        directionUp && !shouldOverlap -> anchorY - (containerView.measuredHeight - anchor.height)
        !directionUp && shouldOverlap -> anchorY - (MENU_CONTAINER_PADDING / 2)
        else -> anchorY
    }

    return if (directionUp) {
        targetY - verticalOffset
    } else {
        targetY + verticalOffset
    }
}
