/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test


import android.graphics.*
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.view.Surface
import org.hamcrest.Matchers.*
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExpectedException
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import java.nio.ByteBuffer

private const val SCREEN_HEIGHT = 100
private const val SCREEN_WIDTH = 100

@RunWith(AndroidJUnit4::class)
@MediumTest
@ReuseSession(false)
class ScreenshotTest : BaseSessionTest() {

    @get:Rule
    val expectedEx: ExpectedException = ExpectedException.none()

    private fun getComparisonScreenshot(width: Int, height: Int): Bitmap {
        val screenshotFile = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(screenshotFile)
        val paint = Paint()
        paint.color = Color.rgb(0, 128, 0)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), paint)
        return screenshotFile
    }

    private fun assertScreenshotResult(result: GeckoResult<Bitmap>, comparisonImage: Bitmap) {
        sessionRule.waitForResult(result).let {
            assertThat("Screenshot is not null",
                    it, notNullValue())
            assertThat("Widths are the same", comparisonImage.width, equalTo(it.width))
            assertThat("Heights are the same", comparisonImage.height, equalTo(it.height))
            assertThat("Byte counts are the same", comparisonImage.byteCount, equalTo(it.byteCount))
            assertThat("Configs are the same", comparisonImage.config, equalTo(it.config))
            val comparisonPixels: ByteBuffer = ByteBuffer.allocate(comparisonImage.byteCount)
            comparisonImage.copyPixelsToBuffer(comparisonPixels)
            val itPixels: ByteBuffer = ByteBuffer.allocate(it.byteCount)
            it.copyPixelsToBuffer(itPixels)
            assertThat("Bytes are the same", comparisonPixels, equalTo(itPixels))
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsSucceeds() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsCanBeCalledMultipleTimes() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.display?.let {
            val call1 = it.capturePixels()
            val call2 = it.capturePixels()
            val call3 = it.capturePixels()
            assertScreenshotResult(call1, screenshotFile)
            assertScreenshotResult(call2, screenshotFile)
            assertScreenshotResult(call3, screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsCompletesCompositorPausedRestarted() {
        sessionRule.display?.let {
            it.surfaceDestroyed()
            val result = it.capturePixels()
            val texture = SurfaceTexture(0)
            texture.setDefaultBufferSize(SCREEN_WIDTH, SCREEN_HEIGHT)
            val surface = Surface(texture)
            it.surfaceChanged(surface, SCREEN_WIDTH, SCREEN_HEIGHT)
            sessionRule.waitForResult(result)
        }
    }

    @Test
    fun capturePixelsThrowsCompositorNotReady() {
        expectedEx.expect(IllegalStateException::class.java)
        expectedEx.expectMessage("Compositor must be ready before pixels can be captured")
        val session = sessionRule.createClosedSession()
        val display = session.acquireDisplay()

        sessionRule.waitForResult(display.capturePixels())
        fail("IllegalStateException expected to be thrown")
    }
}
