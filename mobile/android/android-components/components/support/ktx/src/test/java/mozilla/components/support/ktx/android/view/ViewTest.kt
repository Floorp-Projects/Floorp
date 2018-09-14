/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.view.View
import android.widget.EditText
import android.widget.TextView
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.shadows.ShadowLooper

@RunWith(RobolectricTestRunner::class)
class ViewTest {
    @Test
    fun `showKeyboard should request focus`() {
        val view = EditText(RuntimeEnvironment.application)
        assertFalse(view.hasFocus())

        view.showKeyboard()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks()

        assertTrue(view.hasFocus())
    }

    @Test
    fun `visibility helper methods`() {
        val view = TextView(RuntimeEnvironment.application)

        view.visibility = View.GONE

        assertTrue(view.isGone())
        assertFalse(view.isVisible())
        assertFalse(view.isInvisible())

        view.visibility = View.VISIBLE

        assertFalse(view.isGone())
        assertTrue(view.isVisible())
        assertFalse(view.isInvisible())

        view.visibility = View.INVISIBLE

        assertFalse(view.isGone())
        assertFalse(view.isVisible())
        assertTrue(view.isInvisible())
    }
}
