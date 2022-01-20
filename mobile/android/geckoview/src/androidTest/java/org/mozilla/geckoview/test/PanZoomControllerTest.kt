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
import org.junit.Assume.assumeTrue
import org.mozilla.geckoview.PanZoomController

@RunWith(AndroidJUnit4::class)
@MediumTest
class PanZoomControllerTest : BaseSessionTest() {
    private val errorEpsilon = 3.0
    private val scrollWaitTimeout = 10000.0 // 10 seconds

    private fun setupDocument(documentPath: String) {
        sessionRule.session.loadTestPath(documentPath)
        sessionRule.session.waitForPageStop()
        sessionRule.session.promiseAllPaintsDone()
        sessionRule.session.flushApzRepaints()
    }

    private fun setupScroll() {
        setupDocument(SCROLL_TEST_PATH)
    }

    private fun waitForVisualScroll(offset: Double, timeout: Double, param: String) {
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
        waitForVisualScroll(offset, timeout, "pageLeft")
    }

    private fun waitForVerticalScroll(offset: Double, timeout: Double) {
        waitForVisualScroll(offset, timeout, "pageTop")
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
        setupDocument(TOUCH_HTML_PATH)
    }

    private fun sendDownEvent(x: Float, y: Float): GeckoResult<Int> {
        val downTime = SystemClock.uptimeMillis();
        val down = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, x, y, 0);

        val result = mainSession.panZoomController.onTouchEventForDetailResult(down)
                .map { value -> value!!.handledResult() }
        val up = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, x, y, 0);

        mainSession.panZoomController.onTouchEvent(up)

        return result
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun touchEventForResultWithStaticToolbar() {
        setupTouch()

        // No touch handlers, without scrolling
        var value = sessionRule.waitForResult(sendDownEvent(50f, 15f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_UNHANDLED))

        // Touch handler with preventDefault
        value = sessionRule.waitForResult(sendDownEvent(50f, 45f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

        // Touch handler without preventDefault
        value = sessionRule.waitForResult(sendDownEvent(50f, 75f))
        // Nothing should have done in the event handler and the content is not scrollable,
        // thus the input result should be UNHANDLED, i.e. the dynamic toolbar should NOT
        // move in response to the event.
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_UNHANDLED))

        // No touch handlers, with scrolling
        setupScroll()
        value = sessionRule.waitForResult(sendDownEvent(50f, 25f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))

        // Touch handler with scrolling
        value = sessionRule.waitForResult(sendDownEvent(50f, 75f))
        assertThat("Value should match", value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))
    }

    private fun setupTouchEventDocument(documentPath: String, withEventHandler: Boolean) {
        setupDocument(documentPath + if (withEventHandler) "?event" else "")
    }

    private fun waitForScroll(timeout: Double) {
        mainSession.evaluateJS("""
           const targetWindow = document.querySelector('iframe') ?
               document.querySelector('iframe').contentWindow : window;
           new Promise((resolve, reject) => {
             const start = Date.now();
             function step() {
               if (targetWindow.scrollY == targetWindow.scrollMaxY) {
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

    private fun testTouchEventForResult(withEventHandler: Boolean) {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        // The content height is not greater than "screen height - the dynamic toolbar height".
        setupTouchEventDocument(ROOT_100_PERCENT_HEIGHT_HTML_PATH, withEventHandler)
        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        assertThat("The input result should be UNHANDLED in root_100_percent.html",
                    value, equalTo(PanZoomController.INPUT_RESULT_UNHANDLED))

        // There is a 100% height iframe which is not scrollable.
        setupTouchEventDocument(IFRAME_100_PERCENT_HEIGHT_NO_SCROLLABLE_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // The input result should NOT be handled in the iframe content,
        // should NOT be handled in the root either.
        assertThat("The input result should be UNHANDLED in iframe_100_percent_height_no_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_UNHANDLED))

        // There is a 100% height iframe which is scrollable.
        setupTouchEventDocument(IFRAME_100_PERCENT_HEIGHT_SCROLLABLE_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // The input result should be handled in the iframe content.
        assertThat("The input result should be HANDLED_CONTENT in iframe_100_percent_height_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

        // Scroll to the bottom of the iframe
        mainSession.evaluateJS("""
          const iframe = document.querySelector('iframe');
          iframe.contentWindow.scrollTo({
            left: 0,
            top: iframe.contentWindow.scrollMaxY,
            behavior: 'instant'
          });
        """.trimIndent())
        waitForScroll(scrollWaitTimeout)
        mainSession.flushApzRepaints()

        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // The input result should still be handled in the iframe content.
        assertThat("The input result should be HANDLED_CONTENT in iframe_100_percent_height_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

        // The content height is greater than "screen height - the dynamic toolbar height".
        setupTouchEventDocument(ROOT_98VH_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        assertThat("The input result should be HANDLED in root_98vh.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))

        // The content height is equal to "screen height".
        setupTouchEventDocument(ROOT_100VH_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        assertThat("The input result should be HANDLED in root_100vh.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))

        // There is a 98vh iframe which is not scrollable.
        setupTouchEventDocument(IFRAME_98VH_NO_SCROLLABLE_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // The input result should NOT be handled in the iframe content.
        assertThat("The input result should be HANDLED in iframe_98vh_no_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))

        // There is a 98vh iframe which is scrollable.
        setupTouchEventDocument(IFRAME_98VH_SCROLLABLE_HTML_PATH, withEventHandler)
        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // The input result should be handled in the iframe content initially.
        assertThat("The input result should be HANDLED_CONTENT initially in iframe_98vh_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

        // Scroll to the bottom of the iframe
        mainSession.evaluateJS("""
          const iframe = document.querySelector('iframe');
          iframe.contentWindow.scrollTo({
            left: 0,
            top: iframe.contentWindow.scrollMaxY,
            behavior: 'instant'
          });
        """.trimIndent())
        waitForScroll(scrollWaitTimeout)
        mainSession.flushApzRepaints()

        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        // Now the input result should be handled in the root APZC.
        assertThat("The input result should be HANDLED in iframe_98vh_scrollable.html",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun touchEventForResultWithEventHandler() {
      testTouchEventForResult(true)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun touchEventForResultWithoutEventHandler() {
      testTouchEventForResult(false)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun touchEventForResultWithPreventDefault() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        var files = arrayOf(
            ROOT_100_PERCENT_HEIGHT_HTML_PATH,
            ROOT_98VH_HTML_PATH,
            ROOT_100VH_HTML_PATH,
            IFRAME_100_PERCENT_HEIGHT_NO_SCROLLABLE_HTML_PATH,
            IFRAME_100_PERCENT_HEIGHT_SCROLLABLE_HTML_PATH,
            IFRAME_98VH_SCROLLABLE_HTML_PATH,
            IFRAME_98VH_NO_SCROLLABLE_HTML_PATH)

        for (file in files) {
          setupDocument(file + "?event-prevent")
          var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
          assertThat("The input result should be HANDLED_CONTENT in " + file,
                      value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))

          // Scroll to the bottom edge if it's possible.
          mainSession.evaluateJS("""
            const targetWindow = document.querySelector('iframe') ?
                document.querySelector('iframe').contentWindow : window;
            targetWindow.scrollTo({
              left: 0,
              top: targetWindow.scrollMaxY,
              behavior: 'instant'
            });
          """.trimIndent())
          waitForScroll(scrollWaitTimeout)
          mainSession.flushApzRepaints()

          value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
          assertThat("The input result should be HANDLED_CONTENT in " + file,
                      value, equalTo(PanZoomController.INPUT_RESULT_HANDLED_CONTENT))
        }
    }

    private fun fling(): GeckoResult<Int> {
        val downTime = SystemClock.uptimeMillis();
        val down = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, 50f, 90f, 0)

        val result = mainSession.panZoomController.onTouchEventForDetailResult(down)
                .map { value -> value!!.handledResult() }
        var move = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, 50f, 70f, 0)
        mainSession.panZoomController.onTouchEvent(move)
        move = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, 50f, 30f, 0)
        mainSession.panZoomController.onTouchEvent(move)

        val up = MotionEvent.obtain(
                downTime, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, 50f, 10f, 0)
        mainSession.panZoomController.onTouchEvent(up)
        return result
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun dontCrashDuringFastFling() {
        setupDocument(TOUCHSTART_HTML_PATH)

        fling()
        fling()
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun inputResultForFastFling() {
        // Bug 1687842.
        assumeTrue(false)

        setupDocument(TOUCHSTART_HTML_PATH)

        var value = sessionRule.waitForResult(fling())
        assertThat("The initial input result should be HANDLED",
                   value, equalTo(PanZoomController.INPUT_RESULT_HANDLED))
        // Trigger the next fling during the initial scrolling.
        value = sessionRule.waitForResult(fling())
        assertThat("The input result should be IGNORED during the fast fling",
                   value, equalTo(PanZoomController.INPUT_RESULT_IGNORED))
    }
}
