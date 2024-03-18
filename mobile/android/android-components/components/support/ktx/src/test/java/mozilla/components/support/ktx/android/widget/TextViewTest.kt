/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.widget

import android.view.View
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TextViewTest {

    @Test
    fun `check text size is set to the maximum allowable size specified by the height`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY)

        textView.textSize = 200f
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(94f, textView.textSize)
    }

    @Test
    fun `check text size is not adjusted if it is not larger than the allowable size`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(50, View.MeasureSpec.EXACTLY)

        textView.textSize = 10f
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(10f, textView.textSize)
    }

    @Test
    fun `check adjusted text size takes the default ascender padding into account`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(50, View.MeasureSpec.EXACTLY)

        textView.textSize = 100f
        textView.includeFontPadding = false
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(50f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(44f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.adjustMaxTextSize(heightSpec, 25)
        assertEquals(25f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = false
        textView.adjustMaxTextSize(heightSpec, 25)
        assertEquals(50f, textView.textSize)
    }

    @Test
    fun `check text size is the same as the maximum adjusted text size`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(56, View.MeasureSpec.EXACTLY)

        textView.textSize = 50f
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(50f, textView.textSize)

        textView.textSize = 56f
        textView.includeFontPadding = false
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(56f, textView.textSize)
    }

    @Test
    fun `check custom padding affects text size`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(50, View.MeasureSpec.EXACTLY)

        textView.textSize = 100f
        textView.includeFontPadding = false
        textView.setPadding(5, 5, 5, 5)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(40f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.setPadding(0, 5, 0, 5)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(34f, textView.textSize)

        textView.textSize = 100f
        textView.setPadding(5, 0, 5, 0)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(44f, textView.textSize)

        textView.textSize = 100f
        textView.setPadding(0, 5, 0, 0)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(39f, textView.textSize)
    }

    @Test
    fun `check negative available height results in text size 0`() {
        val textView = TextView(testContext)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(50, View.MeasureSpec.EXACTLY)

        textView.textSize = 100f
        textView.includeFontPadding = false
        textView.setPadding(0, 25, 0, 25)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(100f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.setPadding(0, 0, 0, 0)
        textView.adjustMaxTextSize(heightSpec, 51)
        assertEquals(100f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = false
        textView.setPadding(0, 26, 0, 25)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(100f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.setPadding(0, 25, 0, 25)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(100f, textView.textSize)

        textView.textSize = 100f
        textView.includeFontPadding = true
        textView.setPadding(0, 1000, 0, 1000)
        textView.adjustMaxTextSize(heightSpec)
        assertEquals(100f, textView.textSize)
    }
}
