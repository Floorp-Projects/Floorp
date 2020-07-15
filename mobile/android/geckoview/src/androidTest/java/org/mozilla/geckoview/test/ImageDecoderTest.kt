package org.mozilla.geckoview.test

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult

@MediumTest
@RunWith(AndroidJUnit4::class)
class ImageDecoderTest {

    private fun compareBitmap(expectedLocation: String, actual: Bitmap) {
        val stream = InstrumentationRegistry.getInstrumentation().targetContext.assets
                .open(expectedLocation)

        val expected = BitmapFactory.decodeStream(stream)
        for (x in 0 until actual.height) {
            for (y in 0 until actual.width) {
                assertEquals(expected.getPixel(x, y), actual.getPixel(x, y))
            }
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun decodeNullUri() {
        ImageDecoder.instance().decode(null)
    }

    @Test
    fun decodeIconSvg() {
        val svgNatural = GeckoResult<Void>()
        val svg100 = GeckoResult<Void>()

        ImageDecoder.instance().decode("web_extensions/actions/button/icon.svg").accept { actual ->
            assertEquals(500, actual!!.width)
            assertEquals(500, actual.height)
            svgNatural.complete(null)
        }

        ImageDecoder.instance().decode("web_extensions/actions/button/icon.svg", 100).accept { actual ->
            assertEquals(100, actual!!.width)
            assertEquals(100, actual.height)
            compareBitmap("web_extensions/actions/button/expected.png", actual)
            svg100.complete(null)
        }

        UiThreadUtils.waitForResult(svgNatural)
        UiThreadUtils.waitForResult(svg100)
    }

    @Test
    fun decodeIconPng() {
        val pngNatural = GeckoResult<Void>()
        val png100 = GeckoResult<Void>()
        val png38 = GeckoResult<Void>()
        val png19 = GeckoResult<Void>()

        ImageDecoder.instance().decode("web_extensions/actions/button/geo-19.png").accept { actual ->
            compareBitmap("web_extensions/actions/button/geo-19.png", actual!!)
            pngNatural.complete(null)
        }

        // Raster images shouldn't be upscaled
        ImageDecoder.instance().decode("web_extensions/actions/button/geo-38.png", 100).accept { actual ->
            compareBitmap("web_extensions/actions/button/geo-38.png", actual!!)
            png100.complete(null)
        }

        ImageDecoder.instance().decode("web_extensions/actions/button/geo-38.png", 38).accept { actual ->
            compareBitmap("web_extensions/actions/button/geo-38.png", actual!!)
            png38.complete(null)
        }

        ImageDecoder.instance().decode("web_extensions/actions/button/geo-19.png", 19).accept { actual ->
            compareBitmap("web_extensions/actions/button/geo-19.png", actual!!)
            png19.complete(null)
        }

        UiThreadUtils.waitForResult(pngNatural)
        UiThreadUtils.waitForResult(png100)
        UiThreadUtils.waitForResult(png38)
        UiThreadUtils.waitForResult(png19)
    }

    @Test
    fun decodeIconPngDownscale() {
        val pngNatural = GeckoResult<Void>()
        val png16 = GeckoResult<Void>()

        ImageDecoder.instance().decode("web_extensions/actions/button/red.png").accept { actual ->
            assertEquals(32, actual!!.width)
            assertEquals(32, actual.height)

            for (x in 0 until actual.height) {
                for (y in 0 until actual.width) {
                    assertEquals(Color.RED, actual.getPixel(x, y))
                }
            }
            pngNatural.complete(null)
        }

        ImageDecoder.instance().decode("web_extensions/actions/button/red.png", 16).accept { actual ->
            assertEquals(16, actual!!.width)
            assertEquals(16, actual.height)

            for (x in 0 until actual.height) {
                for (y in 0 until actual.width) {
                    assertEquals(Color.RED, actual.getPixel(x, y))
                }
            }
            png16.complete(null)
        }

        UiThreadUtils.waitForResult(pngNatural)
        UiThreadUtils.waitForResult(png16)
    }
}
