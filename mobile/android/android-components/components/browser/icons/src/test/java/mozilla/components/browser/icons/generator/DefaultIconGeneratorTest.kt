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
    fun getRepresentativeCharacter() = runBlocking {
        val generator = DefaultIconGenerator()

        assertEquals("M", generator.getRepresentativeCharacter("https://mozilla.org"))
        assertEquals("W", generator.getRepresentativeCharacter("http://wikipedia.org"))
        assertEquals("P", generator.getRepresentativeCharacter("http://plus.google.com"))
        assertEquals("E", generator.getRepresentativeCharacter("https://en.m.wikipedia.org/wiki/Main_Page"))

        // Stripping common prefixes
        assertEquals("T", generator.getRepresentativeCharacter("http://www.theverge.com"))
        assertEquals("F", generator.getRepresentativeCharacter("https://m.facebook.com"))
        assertEquals("T", generator.getRepresentativeCharacter("https://mobile.twitter.com"))

        // Special urls
        assertEquals("?", generator.getRepresentativeCharacter("file:///"))
        assertEquals("S", generator.getRepresentativeCharacter("file:///system/"))
        assertEquals("P", generator.getRepresentativeCharacter("ftp://people.mozilla.org/test"))

        // No values
        assertEquals("?", generator.getRepresentativeCharacter(""))

        // Rubbish
        assertEquals("Z", generator.getRepresentativeCharacter("zZz"))
        assertEquals("Ö", generator.getRepresentativeCharacter("ölkfdpou3rkjaslfdköasdfo8"))
        assertEquals("?", generator.getRepresentativeCharacter("_*+*'##"))
        assertEquals("ツ", generator.getRepresentativeCharacter("¯\\_(ツ)_/¯"))
        assertEquals("ಠ", generator.getRepresentativeCharacter("ಠ_ಠ Look of Disapproval"))

        // Non-ASCII
        assertEquals("Ä", generator.getRepresentativeCharacter("http://www.ätzend.de"))
        assertEquals("名", generator.getRepresentativeCharacter("http://名がドメイン.com"))
        assertEquals("C", generator.getRepresentativeCharacter("http://√.com"))
        assertEquals("ß", generator.getRepresentativeCharacter("http://ß.de"))
        assertEquals("Ԛ", generator.getRepresentativeCharacter("http://ԛәлп.com/")) // cyrillic

        // Punycode
        assertEquals("X", generator.getRepresentativeCharacter("http://xn--tzend-fra.de")) // ätzend.de
        assertEquals(
            "X",
            generator.getRepresentativeCharacter("http://xn--V8jxj3d1dzdz08w.com")
        ) // 名がドメイン.com

        // Numbers
        assertEquals("1", generator.getRepresentativeCharacter("https://www.1and1.com/"))

        // IP
        assertEquals("1", generator.getRepresentativeCharacter("https://192.168.0.1"))
    }

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

        val icon = generator.generate(testContext, IconRequest(
            url = "https://m.facebook.com"))

        assertNotNull(icon.bitmap)
        assertNotNull(icon.color)

        val dp32 = 32.dpToPx(testContext.resources.displayMetrics)
        assertEquals(dp32, icon.bitmap.width)
        assertEquals(dp32, icon.bitmap.height)

        assertEquals(Bitmap.Config.ARGB_8888, icon.bitmap.config)

        assertEquals(Icon.Source.GENERATOR, icon.source)
    }
}
