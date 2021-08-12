/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.generator

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DefaultIconGeneratorTest {

    @Test
    fun pickColor() {
        val generator = DefaultIconGenerator()
        val res = testContext.resources

        val color = generator.pickColor(res, "http://m.facebook.com")

        // Color does not change
        for (i in 0..99) {
            assertEquals(color, generator.pickColor(res, "http://m.facebook.com"))
        }

        // Color is stable for "similar" hosts.
        assertEquals(color, generator.pickColor(res, "https://m.facebook.com"))
        assertEquals(color, generator.pickColor(res, "http://facebook.com"))
        assertEquals(color, generator.pickColor(res, "http://www.facebook.com"))
        assertEquals(color, generator.pickColor(res, "http://www.facebook.com/foo/bar/foobar?mobile=1"))

        // Returns a color for an empty string
        assertNotEquals(0, generator.pickColor(res, ""))
    }

    @Test
    fun generate() = runBlocking {
        val generator = DefaultIconGenerator()

        val icon = generator.generate(
            testContext,
            IconRequest(
                url = "https://m.facebook.com"
            )
        )

        assertNotNull(icon.bitmap)
        assertNotNull(icon.color)

        val dp32 = 32.dpToPx(testContext.resources.displayMetrics)
        assertEquals(dp32, icon.bitmap.width)
        assertEquals(dp32, icon.bitmap.height)

        assertEquals(Bitmap.Config.ARGB_8888, icon.bitmap.config)

        assertEquals(Icon.Source.GENERATOR, icon.source)
    }
}
