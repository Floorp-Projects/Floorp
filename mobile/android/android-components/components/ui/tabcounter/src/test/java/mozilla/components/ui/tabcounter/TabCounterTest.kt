/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import android.content.res.ColorStateList
import android.view.LayoutInflater
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounter.Companion.SO_MANY_TABS_OPEN
import mozilla.components.ui.tabcounter.databinding.MozacUiTabcounterLayoutBinding
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TabCounterTest {

    private lateinit var tabCounter: TabCounter
    private lateinit var binding: MozacUiTabcounterLayoutBinding

    @Before
    fun setUp() {
        tabCounter = TabCounter(testContext)
        binding =
            MozacUiTabcounterLayoutBinding.inflate(LayoutInflater.from(testContext), tabCounter)
    }

    @Test
    fun `Default tab count is set to zero`() {
        assertEquals("0", binding.counterText.text)
    }

    @Test
    fun `Set tab count as single digit value shows count`() {
        tabCounter.setCount(1)
        assertEquals("1", binding.counterText.text)
    }

    @Test
    fun `Set tab count as two digit number shows count`() {
        tabCounter.setCount(99)
        assertEquals("99", binding.counterText.text)
    }

    @Test
    fun `Setting tab count as three digit value shows correct icon`() {
        tabCounter.setCount(100)
        assertEquals(SO_MANY_TABS_OPEN, binding.counterText.text)
    }

    @Test
    fun `Setting tab color shows correct icon`() {
        val colorStateList: ColorStateList = mock()

        tabCounter.setColor(colorStateList)
        assertEquals(binding.counterText.textColors, colorStateList)
    }
}
