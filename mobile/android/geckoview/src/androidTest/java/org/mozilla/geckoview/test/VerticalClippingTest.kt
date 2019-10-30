/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.*
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.notNullValue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import android.graphics.Bitmap
import org.hamcrest.Matchers.equalTo
import java.nio.ByteBuffer


private const val SCREEN_HEIGHT = 100
private const val SCREEN_WIDTH = 100
private const val BANNER_HEIGHT = SCREEN_HEIGHT * 0.1f // height: 10%

@RunWith(AndroidJUnit4::class)
@MediumTest
class VerticalClippingTest : BaseSessionTest() {
    private fun getComparisonScreenshot(bottomOffset: Int): Bitmap {
        val screenshotFile = Bitmap.createBitmap(SCREEN_WIDTH, SCREEN_HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(screenshotFile)
        val paint = Paint()

        // Draw body
        paint.color = Color.rgb(0, 0, 255)
        canvas.drawRect(0f, 0f, SCREEN_WIDTH.toFloat(), SCREEN_HEIGHT.toFloat(), paint)

        // Draw bottom banner
        paint.color = Color.rgb(0, 255, 0)
        canvas.drawRect(0f, SCREEN_HEIGHT - BANNER_HEIGHT - bottomOffset,
                SCREEN_WIDTH.toFloat(), (SCREEN_HEIGHT - bottomOffset).toFloat(), paint)

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
            assertThat("Bytes are the same", comparisonPixels, equalTo(itPixels))        }
    }


    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun verticalClippingSucceeds() {
        sessionRule.session.loadTestPath(BaseSessionTest.FIXED_BOTTOM)
        sessionRule.waitForPageStop()

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), getComparisonScreenshot(0))
            it.setVerticalClipping(45)
            assertScreenshotResult(it.capturePixels(), getComparisonScreenshot(45))
        }
    }

}