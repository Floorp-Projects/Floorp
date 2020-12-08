/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.PermissionHighlights.NONE
import mozilla.components.concept.toolbar.Toolbar.PermissionHighlights.AUTOPLAY_BLOCKED
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
class PermissionHighlightsIconViewTest {

    @Test
    fun `after setting tint, can get trackingProtectionTint`() {
        val view = PermissionHighlightsIconView(testContext)
        view.setTint(android.R.color.black)
        assertEquals(android.R.color.black, view.permissionTint)
    }

    @Test
    fun `setting permissionHighlights status will trigger an icon updated`() {
        val view = PermissionHighlightsIconView(testContext)

        view.permissionHighlights = AUTOPLAY_BLOCKED

        assertEquals(AUTOPLAY_BLOCKED, view.permissionHighlights)
        assertTrue(view.isVisible)
        assertNotNull(view.drawable)
        assertEquals(
            view.contentDescription,
            testContext.getString(R.string.mozac_browser_toolbar_content_description_autoplay_blocked)
        )

        view.permissionHighlights = NONE

        assertEquals(NONE, view.permissionHighlights)
        assertNull(view.drawable)
        assertFalse(view.isVisible)
        assertNull(view.contentDescription)
    }

    @Test
    fun `setIcons will trigger an icon updated`() {
        val view = spy(PermissionHighlightsIconView(testContext))

        view.setIcons(DisplayToolbar.Icons.PermissionHighlights(
                testContext.getDrawable(
                        TrackingProtectionIconView.DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED
                )!!))

        verify(view).updateIcon()
    }
}
