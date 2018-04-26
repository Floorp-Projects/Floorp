/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.widget.EditText
import android.widget.TextView
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.shadows.ShadowLooper
import android.util.DisplayMetrics
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals

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
    fun `dp returns same value as manual conversion`() {
        val view = TextView(RuntimeEnvironment.application)

        val resources = RuntimeEnvironment.application.getResources()
        val metrics = resources.getDisplayMetrics()

        for (i in 1..500) {
            val px = (i * (metrics.densityDpi.toFloat() / DisplayMetrics.DENSITY_DEFAULT)).toInt()
            assertEquals(px, view.dp(i))
            assertNotEquals(0, view.dp(i))
        }
    }
}
