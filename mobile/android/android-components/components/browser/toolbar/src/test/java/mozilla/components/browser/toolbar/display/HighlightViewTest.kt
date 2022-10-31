/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.Highlight.NONE
import mozilla.components.concept.toolbar.Toolbar.Highlight.PERMISSIONS_CHANGED
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class HighlightViewTest {

    @Test
    fun `after setting tint, can get trackingProtectionTint`() {
        val view = HighlightView(testContext)
        view.setTint(android.R.color.black)
        assertEquals(android.R.color.black, view.highlightTint)
    }

    @Test
    fun `setting status will trigger an icon updated`() {
        val view = HighlightView(testContext)

        view.state = PERMISSIONS_CHANGED

        assertEquals(PERMISSIONS_CHANGED, view.state)
        assertTrue(view.isVisible)
        assertNotNull(view.drawable)
        assertEquals(
            view.contentDescription,
            testContext.getString(R.string.mozac_browser_toolbar_content_description_autoplay_blocked),
        )

        view.state = NONE

        assertEquals(NONE, view.state)
        assertNull(view.drawable)
        assertFalse(view.isVisible)
        assertNull(view.contentDescription)
    }

    @Test
    fun `setIcons will trigger an icon updated`() {
        val view = spy(HighlightView(testContext))

        view.setIcon(
            testContext.getDrawable(
                TrackingProtectionIconView.DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED,
            )!!,
        )

        verify(view).updateIcon()
    }
}
