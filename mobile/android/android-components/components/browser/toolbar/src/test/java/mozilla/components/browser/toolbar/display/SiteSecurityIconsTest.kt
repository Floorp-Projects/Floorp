/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.graphics.Color
import android.graphics.ColorMatrix
import android.graphics.ColorMatrixColorFilter
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.drawable.Drawable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SiteSecurityIconsTest {

    @Test
    fun `default returns non-null tinted icons`() {
        val icons = SiteSecurityIcons.getDefaultSecurityIcons(testContext, Color.RED)
        assertEquals(PorterDuffColorFilter(Color.RED, PorterDuff.Mode.SRC_IN), icons.insecure?.colorFilter)
        assertEquals(PorterDuffColorFilter(Color.RED, PorterDuff.Mode.SRC_IN), icons.secure?.colorFilter)
    }

    @Test
    fun `withColorFilter tints existing drawables`() {
        val insecure: Drawable = mock()
        val secure: Drawable = mock()
        val icons = SiteSecurityIcons(insecure, secure).withColorFilter(Color.BLUE, Color.RED)

        assertEquals(insecure, icons.insecure)
        assertEquals(secure, icons.secure)
        verify(insecure).colorFilter = PorterDuffColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN)
        verify(secure).colorFilter = PorterDuffColorFilter(Color.RED, PorterDuff.Mode.SRC_IN)
    }

    @Test
    fun `withColorFilter allows custom filters to be used`() {
        val insecure: Drawable = mock()
        val secure: Drawable = mock()
        val filter = ColorMatrixColorFilter(ColorMatrix())
        val icons = SiteSecurityIcons(insecure, secure).withColorFilter(filter, filter)

        assertEquals(insecure, icons.insecure)
        assertEquals(secure, icons.secure)
        verify(insecure).colorFilter = filter
        verify(secure).colorFilter = filter
    }
}
