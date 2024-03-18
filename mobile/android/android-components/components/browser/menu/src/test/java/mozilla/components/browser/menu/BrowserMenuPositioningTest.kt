/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.robolectric.Shadows
import org.robolectric.shadows.ShadowDisplay

@RunWith(AndroidJUnit4::class)
class BrowserMenuPositioningTest {

    @Test
    fun `GIVEN inferMenuPositioningData WHEN called with the menu layout, anchor and current menu data THEN it returns a new MenuPositioningData populated with all data needed to show a PopupWindow`() {
        val view: ViewGroup = mock()
        Mockito.doReturn(70).`when`(view).measuredHeight
        val anchor = View(testContext)
        anchor.layoutParams = ViewGroup.LayoutParams(20, 40)
        setScreenHeight(100)

        val result = inferMenuPositioningData(view, anchor, MenuPositioningData())

        val expected = MenuPositioningData(
            BrowserMenuPlacement.AnchoredToTop.Dropdown(anchor), // orientation DOWN and fitsDown
            askedOrientation = BrowserMenu.Orientation.DOWN, // default
            fitsUp = false, // availableHeightToTop(0) is smaller than containerHeight(70)
            fitsDown = true, // availableHeightToBottom(470) is bigger than containerHeight(70)
            availableHeightToTop = 0,
            availableHeightToBottom = 100, // mocked by us above
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
}
