/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import android.content.res.ColorStateList
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.android.synthetic.main.mozac_ui_tabcounter_layout.view.*
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounter.Companion.SO_MANY_TABS_OPEN
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TabCounterTest {
    @Test
    fun `Default tab count is set to zero`() {
        val tabCounter = TabCounter(testContext)
        assertEquals("0", tabCounter.counter_text.text)
    }

    @Test
    fun `Set tab count as single digit value shows count`() {
        val tabCounter = TabCounter(testContext)
        tabCounter.setCount(1)
        assertEquals("1", tabCounter.counter_text.text)
    }

    @Test
    fun `Set tab count as two digit number shows count`() {
        val tabCounter = TabCounter(testContext)
        tabCounter.setCount(99)
        assertEquals("99", tabCounter.counter_text.text)
    }

    @Test
    fun `Setting tab count as three digit value shows correct icon`() {
        val tabCounter = TabCounter(testContext)
        tabCounter.setCount(100)
        assertEquals(SO_MANY_TABS_OPEN, tabCounter.counter_text.text)
    }

    @Test
    fun `Setting tab color shows correct icon`() {
        val tabCounter = TabCounter(testContext)
        val colorStateList: ColorStateList = mock()

        tabCounter.setColor(colorStateList)
        assertEquals(tabCounter.counter_text.textColors, colorStateList)
    }
}
