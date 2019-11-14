/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.*
import android.graphics.Bitmap
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Base64
import java.io.ByteArrayOutputStream
import org.hamcrest.Matchers.*
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.hamcrest.Matchers.equalTo

private const val SCREEN_WIDTH = 100
private const val SCREEN_HEIGHT = 200

@RunWith(AndroidJUnit4::class)
@MediumTest
class DynamicToolbarTest : BaseSessionTest() {
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun outOfRangeValue() {
        try {
            sessionRule.display?.run { setDynamicToolbarMaxHeight(SCREEN_HEIGHT + 1) }
            fail("Request should have failed")
        } catch (e: AssertionError) {
            assertThat("Throws an exception when setting values greater than the client height",
                       e.toString(), containsString("maximum height of the dynamic toolbar"))
        }
    }

    private fun assertScreenshotResult(result: GeckoResult<Bitmap>, comparisonImage: Bitmap) {
        sessionRule.waitForResult(result).let {
            assertThat("Screenshot is not null",
                    it, notNullValue())
            assertThat("Widths are the same", comparisonImage.width, equalTo(it.width))
            assertThat("Heights are the same", comparisonImage.height, equalTo(it.height))
            assertThat("Byte counts are the same", comparisonImage.byteCount, equalTo(it.byteCount))
            assertThat("Configs are the same", comparisonImage.config, equalTo(it.config))

            if (!comparisonImage.sameAs(it)) {
              val outputForComparison = ByteArrayOutputStream()
              comparisonImage.compress(Bitmap.CompressFormat.PNG, 100, outputForComparison)

              val outputForActual = ByteArrayOutputStream()
              it.compress(Bitmap.CompressFormat.PNG, 100, outputForActual)
              val actualString: String = Base64.encodeToString(outputForActual.toByteArray(), Base64.DEFAULT)
              val comparisonString: String = Base64.encodeToString(outputForComparison.toByteArray(), Base64.DEFAULT)

              assertThat("Encoded strings are the same", comparisonString, equalTo(actualString))
            }

            assertThat("Bytes are the same", comparisonImage.sameAs(it), equalTo(true))
        }
    }

    /**
     * Returns a whole green Bitmap.
     * This Bitmap would be a reference image of tests in this file.
     */
    private fun getComparisonScreenshot(width: Int, height: Int): Bitmap {
        val screenshotFile = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(screenshotFile)
        val paint = Paint()
        paint.color = Color.rgb(0, 128, 0)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), paint)
        return screenshotFile
    }

    // With the dynamic toolbar max height vh units values exceed
    // the top most window height. This is a test case that exceeded area
    // is rendered properly (on the compositor).
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun positionFixedElementClipping() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(SCREEN_HEIGHT / 2) }

        val reference = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        // FIXED_VH is an HTML file which has a position:fixed element whose
        // style is "width: 100%; height: 200vh" and the document is scaled by
        // minimum-scale 0.5, so that the height of the element exceeds the
        // window height.
        mainSession.loadTestPath(BaseSessionTest.FIXED_VH)
        mainSession.waitForPageStop()

        // Scroll down bit, if we correctly render the document, the position
        // fixed element still covers whole the document area.
        mainSession.evaluateJS("window.scrollTo({ top: 100, behevior: 'instant' })")

        // Wait a while to make sure the scrolling result is composited on the compositor
        // since capturePixels() takes a snapshot directly from the compositor without
        // waiting for a corresponding MozAfterPaint on the main-thread so it's possible
        // to take a stale snapshot even if it's a result of syncronous scrolling.
        mainSession.evaluateJS("new Promise(resolve => window.setTimeout(resolve, 1000))")

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), reference)
        }
    }

    // Asynchronous scrolling with the dynamic toolbar max height causes
    // situations where the visual viewport size gets bigger than the layout
    // viewport on the compositor thread because of 200vh position:fixed
    // elements.  This is a test case that a 200vh position element is
    // properly rendered its positions.
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun layoutViewportExpansion() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(SCREEN_HEIGHT / 2) }

        val reference = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        mainSession.loadTestPath(BaseSessionTest.FIXED_VH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("window.scrollTo(0, 100)")

        // Scroll back to the original position by asynchronous scrolling.
        mainSession.evaluateJS("window.scrollTo({ top: 0, behavior: 'smooth' })")

        mainSession.evaluateJS("new Promise(resolve => window.setTimeout(resolve, 1000))")

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), reference)
        }
    }

}
