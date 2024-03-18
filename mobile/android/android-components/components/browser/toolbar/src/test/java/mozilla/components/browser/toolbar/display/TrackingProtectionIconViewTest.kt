/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.drawable.AnimatedVectorDrawable
import android.graphics.drawable.Drawable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TrackingProtectionIconViewTest {

    @Test
    fun `After setting tint, can get trackingProtectionTint`() {
        val view = TrackingProtectionIconView(testContext)
        view.setTint(android.R.color.black)
        assertEquals(android.R.color.black, view.trackingProtectionTint)
    }

    @Test
    fun `colorFilter is cleared on animatable drawables`() {
        val view = TrackingProtectionIconView(testContext)
        view.trackingProtectionTint = android.R.color.black

        val drawable = mock<Drawable>()
        val animatedDrawable = mock<AnimatedVectorDrawable>()

        view.setOrClearColorFilter(drawable)
        assertEquals(PorterDuffColorFilter(android.R.color.black, PorterDuff.Mode.SRC_ATOP), view.colorFilter)

        view.setOrClearColorFilter(animatedDrawable)
        assertNotEquals(PorterDuffColorFilter(android.R.color.black, PorterDuff.Mode.SRC_ATOP), view.colorFilter)
    }
}
