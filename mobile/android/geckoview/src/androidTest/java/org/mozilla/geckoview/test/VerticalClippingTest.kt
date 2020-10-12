/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.*
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.notNullValue
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import android.graphics.Bitmap
import org.hamcrest.Matchers
import org.hamcrest.Matchers.equalTo
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.Callbacks


private const val SCREEN_HEIGHT = 800
private const val SCREEN_WIDTH = 800
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
            assertThat("Images are almost identical",
                    ScreenshotTest.Companion.imageElementDifference(comparisonImage, it),
                    Matchers.lessThanOrEqualTo(1))
        }
    }


    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun verticalClippingSucceeds() {
        // Disable failing test on Webrender. Bug 1670267
        assumeThat(sessionRule.env.isWebrender, equalTo(false))
        sessionRule.display?.setVerticalClipping(45)
        sessionRule.session.loadTestPath(FIXED_BOTTOM)
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), getComparisonScreenshot(45))
        }
    }

}