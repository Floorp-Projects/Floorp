/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.Shadows
import org.robolectric.shadows.ShadowDisplay

@RunWith(AndroidJUnit4::class)
class BrowserMenuPositioningTest {

    @Test
    fun `GIVEN a menu that fitsDown WHEN inferMenuPositioningData is called THEN the menu can be shown with preferred orientation anchored to the top and shown as a dropdown`() {
        val (view, anchor) = setupTest(
            viewHeight = 70,
            anchorLayoutParams = ViewGroup.LayoutParams(20, 40),
            screenHeight = 100,
        )

        val result = inferMenuPositioningData(view, anchor, MenuPositioningData())

        Assert.assertTrue(canUsePreferredOrientation(result))
        Assert.assertFalse(neitherOrientationFits(result))

        val expected = MenuPositioningData(
            BrowserMenuPlacement.AnchoredToTop.Dropdown(anchor), // orientation DOWN and fitsDown
            askedOrientation = BrowserMenu.Orientation.DOWN, // default
            fitsUp = false, // availableHeightToTop(0) is smaller than containerHeight(70)
            fitsDown = true, // availableHeightToBottom(100) is bigger than containerHeight(70)
            availableHeightToTop = 0,
            availableHeightToBottom = 100, // mocked by us above
            containerViewHeight = 70, // mocked by us above
        )
        Assert.assertEquals(expected, result)
    }

    @Test
    fun `GIVEN asked orientation is UP and a menu that only fitsDown WHEN inferMenuPositioningData is called THEN the menu cannot be shown with preferred orientation but anchored to the top and shown as a dropdown`() {
        val (view, anchor) = setupTest(
            viewHeight = 70,
            anchorLayoutParams = ViewGroup.LayoutParams(20, 40),
            screenHeight = 100,
        )

        val result = inferMenuPositioningData(
            view,
            anchor,
            MenuPositioningData(askedOrientation = BrowserMenu.Orientation.UP),
        )

        Assert.assertFalse(canUsePreferredOrientation(result))
        Assert.assertFalse(neitherOrientationFits(result))

        val expected = MenuPositioningData(
            BrowserMenuPlacement.AnchoredToTop.Dropdown(anchor), // orientation DOWN and fitsDown
            askedOrientation = BrowserMenu.Orientation.UP, // requested
            fitsUp = false, // availableHeightToTop(0) is smaller than containerHeight(70)
            fitsDown = true, // availableHeightToBottom(100) is bigger than containerHeight(70)
            availableHeightToTop = 0,
            availableHeightToBottom = 100, // mocked by us above
            containerViewHeight = 70, // mocked by us above
        )
        Assert.assertEquals(expected, result)
    }

    @Test
    fun `GIVEN a menu that does not fit down or up with more space available below the anchor WHEN inferMenuPositioningData is called the menu can be shown anchored to the top and placed at a specific location`() {
        val (view, anchor) = setupTest(
            viewHeight = 60,
            anchorLayoutParams = ViewGroup.LayoutParams(20, 40),
            screenHeight = 50,
        )

        val result = inferMenuPositioningData(view, anchor, MenuPositioningData())

        Assert.assertFalse(canUsePreferredOrientation(result))
        Assert.assertTrue(neitherOrientationFits(result))

        val expected = MenuPositioningData(
            BrowserMenuPlacement.AnchoredToTop.ManualAnchoring(anchor), // orientation DOWN and fitsDown
            askedOrientation = BrowserMenu.Orientation.DOWN, // default
            fitsUp = false, // availableHeightToTop(0) is smaller than containerHeight(70)
            fitsDown = false, // availableHeightToBottom(50) is smaller than containerHeight(70)
            availableHeightToTop = 0,
            availableHeightToBottom = 50, // mocked by us above
            containerViewHeight = 60, // mocked by us above
        )
        Assert.assertEquals(expected, result)
    }

    @Test
    fun `GIVEN a menu that does not fit down or up with more space available above the anchor WHEN inferMenuPositioningData is called THEN the menu can be shown anchored to the bottom and placed at a specific location`() {
        val (view, anchor) = setupTest(
            viewHeight = 70,
            anchorLayoutParams = ViewGroup.LayoutParams(20, 40),
            screenHeight = 100,
        )

        // mock anchor position
        whenever(anchor.getLocationOnScreen(IntArray(2))).thenAnswer { invocation ->
            val args = invocation.arguments
            val location = args[0] as IntArray
            location[0] = 0
            location[1] = 60
            location
        }

        val result = inferMenuPositioningData(view, anchor, MenuPositioningData())

        Assert.assertFalse(canUsePreferredOrientation(result))
        Assert.assertTrue(neitherOrientationFits(result))

        val expected = MenuPositioningData(
            BrowserMenuPlacement.AnchoredToBottom.ManualAnchoring(anchor), // orientation UP and fitsUp
            askedOrientation = BrowserMenu.Orientation.DOWN, // default
            fitsUp = false, // availableHeightToTop(60) is smaller than containerHeight(70)
            fitsDown = false, // availableHeightToBottom(40) is smaller than containerHeight(70)
            availableHeightToTop = 60, // mocked by us above
            availableHeightToBottom = 40,
            containerViewHeight = 70, // mocked by us above
        )

        Assert.assertEquals(expected, result)
    }

    @Test
    fun `GIVEN inferMenuPosition WHEN called with an anchor and the current menu data THEN it returns a new MenuPositioningData with data about positioning the menu`() {
        val view: View = mock()

        var data = MenuPositioningData(askedOrientation = BrowserMenu.Orientation.DOWN, fitsDown = true)
        var result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToTop.Dropdown(view),
            result.inferredMenuPlacement,
        )

        data = MenuPositioningData(askedOrientation = BrowserMenu.Orientation.UP, fitsUp = true)
        result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToBottom.Dropdown(view),
            result.inferredMenuPlacement,
        )

        data = MenuPositioningData(
            fitsUp = false,
            fitsDown = false,
            availableHeightToTop = 1,
            availableHeightToBottom = 2,
        )
        result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToTop.ManualAnchoring(view),
            result.inferredMenuPlacement,
        )

        data = MenuPositioningData(
            fitsUp = false,
            fitsDown = false,
            availableHeightToTop = 1,
            availableHeightToBottom = 0,
        )
        result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToBottom.ManualAnchoring(view),
            result.inferredMenuPlacement,
        )

        data = MenuPositioningData(askedOrientation = BrowserMenu.Orientation.DOWN, fitsUp = true)
        result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToBottom.Dropdown(view),
            result.inferredMenuPlacement,
        )

        data = MenuPositioningData(askedOrientation = BrowserMenu.Orientation.UP, fitsDown = true)
        result = inferMenuPosition(view, data)
        Assert.assertEquals(
            BrowserMenuPlacement.AnchoredToTop.Dropdown(view),
            result.inferredMenuPlacement,
        )
    }

    private fun setScreenHeight(value: Int) {
        val display = ShadowDisplay.getDefaultDisplay()
        val shadow = Shadows.shadowOf(display)
        shadow.setHeight(value)
    }

    private fun setupTest(
        viewHeight: Int,
        anchorLayoutParams: ViewGroup.LayoutParams,
        screenHeight: Int,
    ): Pair<ViewGroup, View> {
        val view: ViewGroup = mock()
        doReturn(viewHeight).`when`(view).measuredHeight
        val anchor = spy(View(testContext))
        anchor.layoutParams = anchorLayoutParams
        setScreenHeight(screenHeight)
        return Pair(view, anchor)
    }
}
