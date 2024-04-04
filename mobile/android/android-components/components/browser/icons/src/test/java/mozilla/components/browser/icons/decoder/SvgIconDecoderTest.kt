/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.caverock.androidsvg.SVGParseException
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.test.any
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class SvgIconDecoderTest {
    private val desiredSize = DesiredSize(
        targetSize = 32,
        minSize = 32,
        maxSize = 256,
        maxScaleFactor = 2.0f,
    )

    private val validSvg: ByteArray = """<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512"></svg>""".toByteArray()

    // SVGParseException for unbalancedClose
    private val invalidSvg: ByteArray = """<svg xmlns="http://www.w3.org/2000/svg"></svg></svg>""".toByteArray()

    // IllegalArgumentException: SVG document is empty
    private val illegalArgumentSvg: ByteArray = """<html lang="en"></html>""".toByteArray()

    // NullPointerException: Attempt to read from field on a null object reference in method
    private val nullPointerSvg: ByteArray = """<!DOCTYPE html><html lang="en"><style>a{background-image:url('data:image/svg+xml,<svg width="24" height="25" viewBox="0 0 24 25" xmlns="http://www.w3.org/2000/svg"><path fill="black" fill-rule="evenodd"/></svg>')}</style></html>""".toByteArray()

    // -------------------------------------------------------------------------------------
    // Tests for SvgIconDecoder.decode
    // -------------------------------------------------------------------------------------
    @Test
    fun `WHEN SVG is valid THEN decode returns non-null bitmap`() {
        val decoder = SvgIconDecoder()

        val bitmap = decoder.decode(
            validSvg,
            desiredSize,
        )

        assertNotNull(bitmap!!)
    }

    @Test
    fun `WHEN decoder throwing NullPointerException THEN returns null`() {
        val decoder = spy(SvgIconDecoder())
        doThrow(NullPointerException()).`when`(decoder).maybeDecode(any(), any())

        val bitmap = decoder.decode(
            ByteArray(0),
            desiredSize,
        )

        assertNull(bitmap)
    }

    @Test
    fun `WHEN decoder throwing IllegalArgumentException THEN returns null`() {
        val decoder = spy(SvgIconDecoder())
        doThrow(IllegalArgumentException()).`when`(decoder).maybeDecode(any(), any())

        val bitmap = decoder.decode(
            ByteArray(0),
            desiredSize,
        )

        assertNull(bitmap)
    }

    @Test
    fun `WHEN out of memory THEN returns null`() {
        val decoder = spy(SvgIconDecoder())
        doThrow(OutOfMemoryError()).`when`(decoder).maybeDecode(any(), any())

        val bitmap = decoder.decode(
            ByteArray(0),
            desiredSize,
        )

        assertNull(bitmap)
    }

    // We couldn't instantiate SVGParseException since it doesn't have public constructor
    @Test
    fun `WHEN SVGParseException THEN returns null`() {
        val decoder = SvgIconDecoder()

        val bitmap = decoder.decode(
            invalidSvg,
            desiredSize,
        )

        assertNull(bitmap)
    }

    // -------------------------------------------------------------------------------------
    // Tests for SvgIconDecoder.maybeDecode
    // -------------------------------------------------------------------------------------
    @Test
    fun `WHEN SVG is valid THEN maybeDecode returns non-null bitmap`() {
        val decoder = SvgIconDecoder()

        val bitmap = decoder.maybeDecode(
            validSvg,
            desiredSize,
        )

        assertNotNull(bitmap)
    }

    @Test
    fun `WHEN parsing error THEN throws SVGParseException`() {
        val decoder = SvgIconDecoder()

        assertThrows(SVGParseException::class.java) {
            decoder.maybeDecode(
                invalidSvg,
                desiredSize,
            )
        }
    }

    @Test
    fun `WHEN input is malformed THEN throws NullPointerException`() {
        val decoder = SvgIconDecoder()

        assertThrows(NullPointerException::class.java) {
            decoder.maybeDecode(
                nullPointerSvg,
                desiredSize,
            )
        }
    }

    @Test
    fun `WHEN input's document is empty THEN throws IllegalArgumentException`() {
        val decoder = SvgIconDecoder()

        assertThrows(IllegalArgumentException::class.java) {
            decoder.maybeDecode(
                illegalArgumentSvg,
                desiredSize,
            )
        }
    }
}
