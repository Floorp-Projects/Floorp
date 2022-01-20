/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test


import android.graphics.*
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.view.Surface
import org.hamcrest.Matchers.*
import org.junit.Assert
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExpectedException
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoResult.OnExceptionListener
import org.mozilla.geckoview.GeckoResult.fromException
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import kotlin.math.absoluteValue
import kotlin.math.max
import android.graphics.BitmapFactory
import android.graphics.Bitmap
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assume.assumeThat
import java.lang.IllegalStateException

private const val SCREEN_HEIGHT = 800
private const val SCREEN_WIDTH = 800
private const val BIG_SCREEN_HEIGHT = 999999
private const val BIG_SCREEN_WIDTH = 999999

@RunWith(AndroidJUnit4::class)
@MediumTest
class ScreenshotTest : BaseSessionTest() {

    @get:Rule
    val expectedEx: ExpectedException = ExpectedException.none()

    private fun getComparisonScreenshot(width: Int, height: Int): Bitmap {
        val screenshotFile = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(screenshotFile)
        val paint = Paint()
        paint.shader = LinearGradient(0f, 0f, width.toFloat(), height.toFloat(), Color.RED, Color.WHITE, Shader.TileMode.MIRROR)
        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), paint)
        return screenshotFile
    }

    companion object {
        /**
         * Compares two Bitmaps and returns the largest color element difference (red, green or blue)
         */
        public fun imageElementDifference(b1: Bitmap, b2: Bitmap): Int {
            return if (b1.width == b2.width && b1.height == b2.height) {
                val pixels1 = IntArray(b1.width * b1.height)
                val pixels2 = IntArray(b2.width * b2.height)
                b1.getPixels(pixels1, 0, b1.width, 0, 0, b1.width, b1.height)
                b2.getPixels(pixels2, 0, b2.width, 0, 0, b2.width, b2.height)
                var maxDiff = 0
                for (i in 0 until pixels1.size) {
                    val redDiff = (Color.red(pixels1[i]) - Color.red(pixels2[i])).absoluteValue
                    val greenDiff = (Color.green(pixels1[i]) - Color.green(pixels2[i])).absoluteValue
                    val blueDiff = (Color.blue(pixels1[i]) - Color.blue(pixels2[i])).absoluteValue
                    maxDiff = max(maxDiff, max(redDiff, max(greenDiff, blueDiff)))
                }
                maxDiff
            } else {
                256
            }
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
            assertThat("Images are almost identical",
                    imageElementDifference(comparisonImage, it), lessThanOrEqualTo(1))
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsSucceeds() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsCanBeCalledMultipleTimes() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

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

    // This tests tries to catch problems like Bug 1644561.
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsStressTest() {
        val screenshots = mutableListOf<GeckoResult<Bitmap>>()
        sessionRule.display?.let {
            for (i in 0..100) {
                screenshots.add(it.capturePixels())
            }

            for (i in 0..50) {
                sessionRule.waitForResult(screenshots[i])
            }

            it.surfaceDestroyed()
            screenshots.add(it.capturePixels())
            it.surfaceDestroyed()

            val texture = SurfaceTexture(0)
            texture.setDefaultBufferSize(SCREEN_WIDTH, SCREEN_HEIGHT)
            val surface = Surface(texture)
            it.surfaceChanged(surface, SCREEN_WIDTH, SCREEN_HEIGHT)

            for (i in 0..100) {
                screenshots.add(it.capturePixels())
            }

            for (i in 0..100) {
                it.surfaceDestroyed()
                screenshots.add(it.capturePixels())
                val newTexture = SurfaceTexture(0)
                newTexture.setDefaultBufferSize(SCREEN_WIDTH, SCREEN_HEIGHT)
                val newSurface = Surface(newTexture)
                it.surfaceChanged(newSurface, SCREEN_WIDTH, SCREEN_HEIGHT)
            }

            try {
                for (result in screenshots) {
                    sessionRule.waitForResult(result)
                }
            } catch (ex: RuntimeException) {
                // Rejecting the screenshot is fine
            }
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test(expected = IllegalStateException::class)
    fun capturePixelsFailsCompositorPaused() {
        sessionRule.display?.let {
            it.surfaceDestroyed()
            val result = it.capturePixels()
            it.surfaceDestroyed()

            sessionRule.waitForResult(result)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun capturePixelsWhileSessionDeactivated() {
        // TODO: Bug 1673955
        assumeThat(sessionRule.env.isFission, equalTo(false))
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.session.setActive(false)

        // Deactivating the session should trigger a flush state change
        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onSessionStateChange(session: GeckoSession,
                                              sessionState: GeckoSession.SessionState) {}
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.capturePixels(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotToBitmap() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot().capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotScaledToSize() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH/2, SCREEN_HEIGHT/2)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot().size(SCREEN_WIDTH/2, SCREEN_HEIGHT/2).capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenShotScaledWithScale() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH/2, SCREEN_HEIGHT/2)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot().scale(0.5f).capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenShotScaledWithAspectPreservingSize() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH/2, SCREEN_HEIGHT/2)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot().aspectPreservingSize(SCREEN_WIDTH/2).capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun recycleBitmap() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            val call1 = it.screenshot().capture()
            assertScreenshotResult(call1, screenshotFile)
            val call2 = it.screenshot().bitmap(call1.poll(1000)).capture()
            assertScreenshotResult(call2, screenshotFile)
            val call3 = it.screenshot().bitmap(call2.poll(1000)).capture()
            assertScreenshotResult(call3, screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotWholeRegion() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH, SCREEN_HEIGHT)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot().source(0,0,SCREEN_WIDTH, SCREEN_HEIGHT).capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotWholeRegionScaled() {
        val screenshotFile = getComparisonScreenshot(SCREEN_WIDTH/2, SCREEN_HEIGHT/2)

        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(it.screenshot()
                    .source(0,0,SCREEN_WIDTH, SCREEN_HEIGHT)
                    .size(SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
                    .capture(), screenshotFile)
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotQuarters() {
        val res = InstrumentationRegistry.getInstrumentation().targetContext.resources
        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(
                    it.screenshot()
                            .source(0,0,SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
                            .capture(), BitmapFactory.decodeResource(res, R.drawable.colors_tl))
            assertScreenshotResult(
                    it.screenshot()
                            .source(SCREEN_WIDTH/2,SCREEN_HEIGHT/2,SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
                            .capture(), BitmapFactory.decodeResource(res, R.drawable.colors_br))
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun screenshotQuartersScaled() {
        val res = InstrumentationRegistry.getInstrumentation().targetContext.resources
        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })

        sessionRule.display?.let {
            assertScreenshotResult(
                    it.screenshot()
                            .source(0,0,SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
                            .size(SCREEN_WIDTH/4, SCREEN_WIDTH/4)
                            .capture(), BitmapFactory.decodeResource(res, R.drawable.colors_tl_scaled))
            assertScreenshotResult(
                    it.screenshot()
                            .source(SCREEN_WIDTH/2,SCREEN_HEIGHT/2,SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
                            .size(SCREEN_WIDTH/4, SCREEN_WIDTH/4)
                            .capture(), BitmapFactory.decodeResource(res, R.drawable.colors_br_scaled))
        }
    }

    @WithDisplay(height = BIG_SCREEN_HEIGHT, width = BIG_SCREEN_WIDTH)
    @Test
    fun giantScreenshot() {
        sessionRule.session.loadTestPath(COLORS_HTML_PATH)
        sessionRule.display?.screenshot()!!.source(0,0, BIG_SCREEN_WIDTH, BIG_SCREEN_HEIGHT)
                .size(BIG_SCREEN_WIDTH, BIG_SCREEN_HEIGHT)
                .capture()
                .exceptionally(OnExceptionListener<Throwable> { error: Throwable ->
                    Assert.assertTrue(error is OutOfMemoryError)
                    fromException(error)
                })
    }
}
