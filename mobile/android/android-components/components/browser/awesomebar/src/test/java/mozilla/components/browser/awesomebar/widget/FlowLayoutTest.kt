/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.widget

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FlowLayoutTest {

    @Test
    fun `Layouts children in multiple rows`() {
        val flowLayout = FlowLayout(testContext)
        flowLayout.spacing = 10

        val view1 = spy(View(testContext)).also { flowLayout.addView(it, 100, 50) }
        val view2 = spy(View(testContext)).also { flowLayout.addView(it, 200, 50) }
        val view3 = spy(View(testContext)).also { flowLayout.addView(it, 300, 50) }
        val view4 = spy(View(testContext)).also { flowLayout.addView(it, 200, 50) }
        val view5 = spy(View(testContext)).also { flowLayout.addView(it, 100, 50) }
        val view6 = spy(View(testContext)).also { flowLayout.addView(it, 100, 50) }

        val widthSpec = View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)

        flowLayout.measure(widthSpec, heightSpec)

        assertEquals(100, view1.measuredWidth)
        assertEquals(50, view1.measuredHeight)

        assertEquals(200, view2.measuredWidth)
        assertEquals(50, view2.measuredHeight)

        assertEquals(300, view3.measuredWidth)
        assertEquals(50, view3.measuredHeight)

        assertEquals(200, view4.measuredWidth)
        assertEquals(50, view4.measuredHeight)

        assertEquals(100, view5.measuredWidth)
        assertEquals(50, view5.measuredHeight)

        assertEquals(100, view6.measuredWidth)
        assertEquals(50, view6.measuredHeight)

        // Expected Layout:

        //   [100] + 10 + [200]              | = 310
        //   [300] + 10 + [200]              | = 510 (Width)
        //   [100] + 10 + [100]              | = 210
        // ----------------------------------+
        // Height = (3 * 50) + (2 x 10) = 170

        assertEquals(510, flowLayout.measuredWidth)
        assertEquals(170, flowLayout.measuredHeight)

        flowLayout.layout(50, 50, 50 + flowLayout.measuredWidth, 50 + flowLayout.measuredHeight)

        // Row 1

        verify(view1).layout(0, 0, 100, 50)
        verify(view2).layout(110, 0, 310, 50)

        // Row 2

        verify(view3).layout(0, 60, 300, 110)
        verify(view4).layout(310, 60, 510, 110)

        // Row 3

        verify(view5).layout(0, 120, 100, 170)
        verify(view6).layout(110, 120, 210, 170)
    }
}
