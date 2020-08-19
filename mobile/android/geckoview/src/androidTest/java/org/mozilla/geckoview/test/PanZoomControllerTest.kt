package org.mozilla.geckoview.test

import android.os.SystemClock
import android.view.MotionEvent
import org.mozilla.geckoview.ScreenLength
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.PanZoomController
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.Callbacks

@RunWith(AndroidJUnit4::class)
@MediumTest
class PanZoomControllerTest : BaseSessionTest() {
    private val errorEpsilon = 3.0
    private val scrollWaitTimeout = 10000.0 // 10 seconds

    private fun setupScroll() {
        sessionRule.setPrefsUntilTestEnd(mapOf("dom.visualviewport.enabled" to true))
        sessionRule.session.loadTestPath(SCROLL_TEST_PATH)
        sessionRule.waitForPageStop()
    }

    private fun waitForScroll(offset: Double, timeout: Double, param: String) {
        mainSession.evaluateJS("""
           new Promise((resolve, reject) => {
             const start = Date.now();
             function step() {
               if (window.visualViewport.$param >= ($offset - $errorEpsilon)) {
                 resolve();
               } else if ($timeout < (Date.now() - start)) {
                 reject();
               } else {
                 window.requestAnimationFrame(step);
               }
             }
             window.requestAnimationFrame(step);
           });
        """.trimIndent())
    }

    private fun waitForHorizontalScroll(offset: Double, timeout: Double) {
        waitForScroll(offset, timeout, "pageLeft")
    }

    private fun waitForVerticalScroll(offset: Double, timeout: Double) {
        waitForScroll(offset, timeout, "pageTop")
    }


    private fun scrollByVertical(mode: Int) {
        setupScroll()
        val vh = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height is not zero", vh, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollBy(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh, scrollWaitTimeout)
        val scrollY = mainSession.evaluateJS("window.visualViewport.pageTop") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh, errorEpsilon))
    }


    private fun scrollByHorizontal(mode: Int) {
        setupScroll()
        val vw = mainSession.evaluateJS("window.visualViewport.width") as Double
        assertThat("Visual viewport width is not zero", vw, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollBy(ScreenLength.fromVisualViewportWidth(1.0), ScreenLength.zero(), mode)
        waitForHorizontalScroll(vw, scrollWaitTimeout)
        val scrollX = mainSession.evaluateJS("window.visualViewport.pageLeft") as Double
        assertThat("scrollBy should have scrolled along x axis one viewport", scrollX, closeTo(vw, errorEpsilon))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByHorizontalSmooth() {
        scrollByHorizontal(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByHorizontalAuto() {
        scrollByHorizontal(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByVerticalSmooth() {
        scrollByVertical(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByVerticalAuto() {
        scrollByVertical(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    private fun scrollByVerticalTwice(mode: Int) {
        setupScroll()
        val vh = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height is not zero", vh, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollBy(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh, scrollWaitTimeout)
        sessionRule.session.panZoomController.scrollBy(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh * 2.0, scrollWaitTimeout)
        val scrollY = mainSession.evaluateJS("window.visualViewport.pageTop") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh * 2.0, errorEpsilon))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByVerticalTwiceSmooth() {
        scrollByVerticalTwice(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollByVerticalTwiceAuto() {
        scrollByVerticalTwice(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    private fun scrollToVertical(mode: Int) {
        setupScroll()
        val vh = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height is not zero", vh, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh, scrollWaitTimeout)
        val scrollY = mainSession.evaluateJS("window.visualViewport.pageTop") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh, errorEpsilon))
    }


    private fun scrollToHorizontal(mode: Int) {
        setupScroll()
        val vw = mainSession.evaluateJS("window.visualViewport.width") as Double
        assertThat("Visual viewport width is not zero", vw, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollTo(ScreenLength.fromVisualViewportWidth(1.0), ScreenLength.zero(), mode)
        waitForHorizontalScroll(vw, scrollWaitTimeout)
        val scrollX = mainSession.evaluateJS("window.visualViewport.pageLeft") as Double
        assertThat("scrollBy should have scrolled along x axis one viewport", scrollX, closeTo(vw, errorEpsilon))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToHorizontalSmooth() {
        scrollToHorizontal(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToHorizontalAuto() {
        scrollToHorizontal(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalSmooth() {
        scrollToVertical(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalAuto() {
        scrollToVertical(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    private fun scrollToVerticalOnZoomedContent(mode: Int) {
        setupScroll()

        val originalVH = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height is not zero", originalVH, greaterThan(0.0))

        val innerHeight = mainSession.evaluateJS("window.innerHeight") as Double
        assertThat("Visual viewport height equals to window.innerHeight", originalVH, equalTo(innerHeight))

        val originalScale = mainSession.evaluateJS("visualViewport.scale") as Double
        assertThat("Visual viewport scale is the initial scale", originalScale, closeTo(0.5, 0.01))

        // Change the resolution so that the visual viewport will be different from the layout viewport.
        sessionRule.setResolutionAndScaleTo(2.0f)

        val scale = mainSession.evaluateJS("visualViewport.scale") as Double
        assertThat("Visual viewport scale is now greater than the initial scale", scale, greaterThan(originalScale))

        val vh = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height has been changed", vh, lessThan(originalVH))

        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)

        waitForVerticalScroll(vh, scrollWaitTimeout)
        val scrollY = mainSession.evaluateJS("window.visualViewport.pageTop") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh, errorEpsilon))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalOnZoomedContentSmooth() {
        scrollToVerticalOnZoomedContent(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalOnZoomedContentAuto() {
        scrollToVerticalOnZoomedContent(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    private fun scrollToVerticalTwice(mode: Int) {
        setupScroll()
        val vh = mainSession.evaluateJS("window.visualViewport.height") as Double
        assertThat("Visual viewport height is not zero", vh, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh, scrollWaitTimeout)
        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.fromVisualViewportHeight(1.0), mode)
        waitForVerticalScroll(vh, scrollWaitTimeout)
        val scrollY = mainSession.evaluateJS("window.visualViewport.pageTop") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh, errorEpsilon))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalTwiceSmooth() {
        scrollToVerticalTwice(PanZoomController.SCROLL_BEHAVIOR_SMOOTH)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollToVerticalTwiceAuto() {
        scrollToVerticalTwice(PanZoomController.SCROLL_BEHAVIOR_AUTO)
    }

    private fun setupTouch() {
        mainSession.loadTestPath(TOUCH_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.ContentDelegate {
            @GeckoSessionTestRule.AssertCalled(count = 1)
            override fun onFirstContentfulPaint(session: GeckoSession) {
            }
        })
    }

    private fun sendDownEvent(x: Float, y: Float): GeckoResult<Int> {
        val downTime = SystemClock.uptimeMillis();
        val down = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, x, y, 0);

        val result = mainSession.panZoomController.onTouchEventForResult(down)

        val up = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, y, 0);

        mainSession.panZoomController.onTouchEvent(up)

        return result
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun touchEventForResult() {
        setupTouch()

        // No touch handlers, without scrolling
        var value = sessionRule.waitForResult(sendDownEvent(50f, 15f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_UNHANDLED))

        // Touch handler with preventDefault
        value = sessionRule.waitForResult(sendDownEvent(50f, 45f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

        // Touch handler without preventDefault
        value = sessionRule.waitForResult(sendDownEvent(50f, 75f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))

        // No touch handlers, with scrolling
        setupScroll()
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))
    }
}
