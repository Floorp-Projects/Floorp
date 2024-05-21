/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.debugsettings.tabs.MAX_TABS_GENERATED
import org.mozilla.fenix.debugsettings.tabs.validateTextField
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabToolsTabCreationErrorTest {

    @Test
    fun `WHEN the text is empty THEN the empty text error is displayed`() {
        val input = ""
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_empty_error)
    }

    @Test
    fun `WHEN the text has a decimal THEN the non digits error is displayed`() {
        val input = "1.1"
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_non_digits_error)
    }

    @Test
    fun `WHEN the text is a negative number THEN the non digits error is displayed`() {
        val input = "-1"
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_non_digits_error)
    }

    @Test
    fun `WHEN the text contains letters or symbols THEN the non digits error is displayed`() {
        val input = "abc!-"
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_non_digits_error)
    }

    @Test
    fun `WHEN the text contains spaces THEN the non digits error is displayed`() {
        val input = " "
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_non_digits_error)
    }

    @Test
    fun `WHEN the text is equal to MAX_TABS_GENERATED THEN no error is displayed`() {
        val input = MAX_TABS_GENERATED.toString()
        val result = validateTextField(input)
        assertNull(result)
    }

    @Test
    fun `WHEN the text is an integer greater than MAX_TABS_GENERATED THEN the exceed max error is displayed`() {
        val input = (MAX_TABS_GENERATED + 1).toString()
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_exceed_max_error)
    }

    @Test
    fun `WHEN the text is 0 THEN the non zero error is displayed`() {
        val input = "0"
        val result = validateTextField(input)
        assertEquals(result, R.string.debug_drawer_tab_tools_tab_quantity_non_zero_error)
    }

    @Test
    fun `WHEN the text is an integer less than MAX_TABS_GENERATED THEN no error is displayed`() {
        val input = (MAX_TABS_GENERATED - 1).toString()
        val result = validateTextField(input)
        assertNull(result)
    }
}
