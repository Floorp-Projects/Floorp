/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.widget.LinearLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ActionSpaceTest {

    @Test
    fun `Toolbar ActionSpace must set padding`() {
        var space = Toolbar.ActionSpace(0)
        val linearLayout = LinearLayout(testContext)
        var view = space.createView(linearLayout)

        assertEquals(view.paddingLeft, 0)
        assertEquals(view.paddingTop, 0)
        assertEquals(view.paddingRight, 0)
        assertEquals(view.paddingBottom, 0)

        space = Toolbar.ActionSpace(
            0,
            padding = Padding(16, 20, 24, 28),
        )

        view = space.createView(linearLayout)
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }

    @Test
    fun `bind is not implemented`() {
        val button = Toolbar.ActionSpace(0)
        assertEquals(Unit, button.bind(mock()))
    }
}
