/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BrowserMenuHighlightTest {

    @Test
    fun `low priority effect keeps notification tint`() {
        val highlight = BrowserMenuHighlight.LowPriority(
            notificationTint = Color.RED,
        )
        assertEquals(LowPriorityHighlightEffect(Color.RED), highlight.asEffect(mock()))
    }

    @Test
    fun `high priority effect keeps background tint`() {
        val highlight = BrowserMenuHighlight.HighPriority(
            backgroundTint = Color.RED,
        )
        assertEquals(HighPriorityHighlightEffect(Color.RED), highlight.asEffect(mock()))
    }

    @Suppress("Deprecation")
    @Test
    fun `classic highlight effect converts background tint`() {
        val highlight = BrowserMenuHighlight.ClassicHighlight(
            startImageResource = 0,
            endImageResource = 0,
            backgroundResource = 0,
            colorResource = R.color.photonRed50,
        )
        assertEquals(HighPriorityHighlightEffect(testContext.getColor(R.color.photonRed50)), highlight.asEffect(testContext))
    }
}
