/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.graphics.*
import android.graphics.Bitmap
import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import android.util.Base64
import java.io.ByteArrayOutputStream
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.ScrollDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.hamcrest.Matchers.closeTo
import org.hamcrest.Matchers.equalTo

private const val SCREEN_WIDTH = 100
private const val SCREEN_HEIGHT = 200

@RunWith(AndroidJUnit4::class)
@MediumTest
class DynamicToolbarTest : BaseSessionTest() {
    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    // Makes sure we can load a page when the dynamic toolbar is bigger than the whole content
    fun outOfRangeValue() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT + 1
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(HELLO_HTML_PATH)
        mainSession.waitForPageStop()
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
        mainSession.evaluateJS("window.scrollTo({ top: 100, behavior: 'instant' })")

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

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun visualViewportEvents() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(BaseSessionTest.FIXED_VH)
        mainSession.waitForPageStop()

        val pixelRatio = sessionRule.session.evaluateJS("window.devicePixelRatio") as Double
        val scale = sessionRule.session.evaluateJS("window.visualViewport.scale") as Double

        for (i in 1..dynamicToolbarMaxHeight) {
          // Simulate the dynamic toolbar is going to be hidden.
          sessionRule.display?.run { setVerticalClipping(-i) }

          val expectedViewportHeight = (SCREEN_HEIGHT - dynamicToolbarMaxHeight + i) / scale / pixelRatio
          val promise = sessionRule.session.evaluatePromiseJS("""
             new Promise(resolve => {
               window.visualViewport.addEventListener('resize', resolve(window.visualViewport.height));
             });
          """.trimIndent())

          assertThat("The visual viewport height should be changed in response to the dynamc toolbar transition",
                     promise.value as Double, closeTo(expectedViewportHeight, .01))
        }
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun percentBaseValueOnPositionFixedElement() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(BaseSessionTest.FIXED_PERCENT)
        mainSession.waitForPageStop()

        val originalHeight = mainSession.evaluateJS("""
            getComputedStyle(document.querySelector('#fixed-element')).height
        """.trimIndent()) as String

        // Set the vertical clipping value to the middle of toolbar transition.
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight / 2) }

        var height = mainSession.evaluateJS("""
            getComputedStyle(document.querySelector('#fixed-element')).height
        """.trimIndent()) as String

        assertThat("The %-based height should be the static in the middle of toolbar tansition",
                   height, equalTo(originalHeight))

        // Set the vertical clipping value to hide the toolbar completely.
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }
        height = mainSession.evaluateJS("""
            getComputedStyle(document.querySelector('#fixed-element')).height
        """.trimIndent()) as String

        val scale = sessionRule.session.evaluateJS("window.visualViewport.scale") as Double
        val expectedHeight = (SCREEN_HEIGHT / scale).toInt()
        assertThat("The %-based height should be now recomputed based on the screen height",
                   height, equalTo(expectedHeight.toString() + "px"))
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun resizeEvents() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(BaseSessionTest.FIXED_VH)
        mainSession.waitForPageStop()

        for (i in 1..dynamicToolbarMaxHeight - 1) {
            val promise = sessionRule.session.evaluatePromiseJS("""
                new Promise(resolve => {
                    let fired = false;
                    window.addEventListener('resize', () => { fired = true; }, { once: true });
                    // Note that `resize` event is fired just before rAF callbacks, so under ideal
                    // circumstances waiting for a rAF should be sufficient, even if it's not sufficient
                    // unexpected resize event(s) will be caught in the next loop.
                    requestAnimationFrame(() => { resolve(fired); });
                });
            """.trimIndent())

            // Simulate the dynamic toolbar is going to be hidden.
            sessionRule.display?.run { setVerticalClipping(-i) }
            assertThat("'resize' event on window should not be fired in response to the dynamc toolbar transition",
                       promise.value as Boolean, equalTo(false));
        }

        val promise = sessionRule.session.evaluatePromiseJS("""
            new Promise(resolve => {
                window.addEventListener('resize', () => { resolve(true); }, { once: true });
            });
        """.trimIndent())

        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }
        assertThat("'resize' event on window should be fired when the dynamc toolbar is completely hidden",
                   promise.value as Boolean, equalTo(true))
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun windowInnerHeight() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        // We intentionally use FIXED_BOTTOM instead of FIXED_VH in this test since
        // FIXED_VH has `minimum-scale=0.5` thus we can't properly test window.innerHeight
        // with FXIED_VH for now due to bug 1598487.
        mainSession.loadTestPath(BaseSessionTest.FIXED_BOTTOM)
        mainSession.waitForPageStop()

        val pixelRatio = sessionRule.session.evaluateJS("window.devicePixelRatio") as Double

        for (i in 1..dynamicToolbarMaxHeight - 1) {
            val promise = sessionRule.session.evaluatePromiseJS("""
               new Promise(resolve => {
                 window.visualViewport.addEventListener('resize', resolve(window.innerHeight));
               });
            """.trimIndent())

            // Simulate the dynamic toolbar is going to be hidden.
            sessionRule.display?.run { setVerticalClipping(-i) }
            assertThat("window.innerHeight should not be changed in response to the dynamc toolbar transition",
                       promise.value as Double, closeTo(SCREEN_HEIGHT / 2 / pixelRatio, .01))
        }

        val promise = sessionRule.session.evaluatePromiseJS("""
            new Promise(resolve => {
                window.addEventListener('resize', () => { resolve(window.innerHeight); }, { once: true });
            });
        """.trimIndent())

        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }
        assertThat("window.innerHeight should be changed when the dynamc toolbar is completely hidden",
                   promise.value as Double, closeTo(SCREEN_HEIGHT / pixelRatio, .01))
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test
    fun notCrashOnResizeEvent() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(BaseSessionTest.FIXED_VH)
        mainSession.waitForPageStop()

        val promise = sessionRule.session.evaluatePromiseJS("""
            new Promise(resolve => window.addEventListener('resize', () => resolve(true)));
        """.trimIndent())

        // Do some setVerticalClipping calls that we might try to queue two window resize events.
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight + 1) }
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }

        assertThat("Got a rezie event", promise.value as Boolean, equalTo(true))
    }

    @WithDisplay(height = SCREEN_HEIGHT, width = SCREEN_WIDTH)
    @Test fun showDynamicToolbar() {
        val dynamicToolbarMaxHeight = SCREEN_HEIGHT / 2
        sessionRule.display?.run { setDynamicToolbarMaxHeight(dynamicToolbarMaxHeight) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        mainSession.loadTestPath(SHOW_DYNAMIC_TOOLBAR_HTML_PATH)
        mainSession.waitForPageStop()
        mainSession.evaluateJS("window.scrollTo(0, " + dynamicToolbarMaxHeight + ")")
        mainSession.waitUntilCalled(object : ScrollDelegate {
            @AssertCalled(count = 1)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })

        // Simulate the dynamic toolbar being hidden by the scroll
        sessionRule.display?.run { setVerticalClipping(-dynamicToolbarMaxHeight) }

        mainSession.synthesizeTap(5, 25)

        mainSession.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onShowDynamicToolbar(session: GeckoSession) {
            }
        })

    }
}
