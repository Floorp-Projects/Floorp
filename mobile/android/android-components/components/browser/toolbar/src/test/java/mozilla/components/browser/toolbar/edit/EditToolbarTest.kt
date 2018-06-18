/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import mozilla.components.browser.toolbar.BrowserToolbar
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class EditToolbarTest {
    @Test
    fun `entered text is forwarded to autocomplete filter`() {
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        toolbar.editToolbar.urlView.onAttachedToWindow()

        var invokedWithParams: List<Any?>? = null
        toolbar.setAutocompleteFilter { p1, p2 ->
            invokedWithParams = listOf(p1, p2)
        }

        toolbar.editToolbar.urlView.setText("Hello World")

        assertEquals(listOf("Hello World", null), invokedWithParams)
    }
}
