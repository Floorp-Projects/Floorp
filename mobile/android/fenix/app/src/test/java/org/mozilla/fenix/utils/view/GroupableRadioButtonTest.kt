/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils.view

import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifySequence
import org.junit.Test

class GroupableRadioButtonTest {

    @Test
    fun `test add 1 radio to group`() {
        val radio = mockk<GroupableRadioButton>(relaxed = true)
        addToRadioGroup(radio)
        verify { radio wasNot Called }
    }

    @Test
    fun `test add 2 radios to group`() {
        val radio1 = mockk<GroupableRadioButton>(relaxed = true)
        val radio2 = mockk<GroupableRadioButton>(relaxed = true)
        addToRadioGroup(radio1, radio2)

        verifySequence {
            radio1.addToRadioGroup(radio2)
            radio2.addToRadioGroup(radio1)
        }
    }

    @Test
    fun `test add 3 radios to group`() {
        val radio1 = mockk<GroupableRadioButton>(relaxed = true)
        val radio2 = mockk<GroupableRadioButton>(relaxed = true)
        val radio3 = mockk<GroupableRadioButton>(relaxed = true)
        addToRadioGroup(radio1, radio2, radio3)

        verifySequence {
            radio1.addToRadioGroup(radio2)
            radio2.addToRadioGroup(radio1)

            radio1.addToRadioGroup(radio3)
            radio3.addToRadioGroup(radio1)

            radio2.addToRadioGroup(radio3)
            radio3.addToRadioGroup(radio2)
        }
    }

    @Test
    fun `test uncheck all`() {
        val radio1 = mockk<GroupableRadioButton>(relaxed = true)
        val radio2 = mockk<GroupableRadioButton>(relaxed = true)
        val radio3 = mockk<GroupableRadioButton>(relaxed = true)
        listOf(radio1, radio2, radio3).uncheckAll()

        verifySequence {
            radio1.updateRadioValue(false)
            radio2.updateRadioValue(false)
            radio3.updateRadioValue(false)
        }
    }
}
