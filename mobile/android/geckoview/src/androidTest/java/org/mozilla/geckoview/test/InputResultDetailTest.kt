package org.mozilla.geckoview.test

import android.os.SystemClock
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.PanZoomController
import org.mozilla.geckoview.PanZoomController.InputResultDetail
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

@RunWith(AndroidJUnit4::class)
@MediumTest
class InputResultDetailTest : BaseSessionTest() {
    private val scrollWaitTimeout = 10000.0 // 10 seconds

    private fun setupDocument(documentPath: String) {
        mainSession.loadTestPath(documentPath)
        mainSession.waitForPageStop()
        mainSession.promiseAllPaintsDone()
        mainSession.flushApzRepaints()
    }

    private fun sendDownEvent(x: Float, y: Float): GeckoResult<InputResultDetail> {
        val downTime = SystemClock.uptimeMillis()
        val down = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_DOWN,
            x,
            y,
            0,
        )

        val result = mainSession.panZoomController.onTouchEventForDetailResult(down)

        val up = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_UP,
            x,
            y,
            0,
        )

        mainSession.panZoomController.onTouchEvent(up)

        return result
    }

    private fun assertResultDetail(
        testName: String,
        actual: InputResultDetail,
        expectedHandledResult: Int,
        expectedScrollableDirections: Int,
        expectedOverscrollDirections: Int,
    ) {
        assertThat(
            testName + ": The handled result",
            actual.handledResult(),
            equalTo(expectedHandledResult),
        )
        assertThat(
            testName + ": The scrollable directions",
            actual.scrollableDirections(),
            equalTo(expectedScrollableDirections),
        )
        assertThat(
            testName + ": The overscroll directions",
            actual.overscrollDirections(),
            equalTo(expectedOverscrollDirections),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testTouchAction() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        for (descendants in arrayOf("subframe", "svg", "nothing")) {
            for (scrollable in arrayOf(true, false)) {
                for (event in arrayOf(true, false)) {
                    for (touchAction in arrayOf("auto", "none", "pan-x", "pan-y")) {
                        var url = TOUCH_ACTION_HTML_PATH + "?"
                        when (descendants) {
                            "subframe" -> url += "descendants=subframe&"
                            "svg" -> url += "descendants=svg&"
                            "nothing" -> {}
                        }
                        if (scrollable) {
                            url += "scrollable&"
                        }
                        if (event) {
                            url += "event&"
                        }
                        url += ("touch-action=" + touchAction)

                        setupDocument(url)

                        // Since sendDownEvent() just sends a touch-down, APZ doesn't
                        // yet know the direction, hence it allows scrolling in both
                        // the pan-x and pan-y cases.
                        var expectedPlace = if (touchAction == "none") {
                            PanZoomController.INPUT_RESULT_HANDLED_CONTENT
                        } else if (scrollable && descendants != "subframe") {
                            PanZoomController.INPUT_RESULT_HANDLED
                        } else {
                            PanZoomController.INPUT_RESULT_UNHANDLED
                        }

                        var expectedScrollableDirections = if (scrollable) {
                            PanZoomController.SCROLLABLE_FLAG_BOTTOM
                        } else {
                            PanZoomController.SCROLLABLE_FLAG_NONE
                        }

                        var expectedOverscrollDirections = if (touchAction == "none") {
                            PanZoomController.OVERSCROLL_FLAG_NONE
                        } else {
                            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL)
                        }

                        var value = sessionRule.waitForResult(sendDownEvent(50f, 20f))
                        assertResultDetail(
                            "`descendants=$descendants, scrollable=$scrollable, event=$event, touch-action=$touchAction`",
                            value,
                            expectedPlace,
                            expectedScrollableDirections,
                            expectedOverscrollDirections,
                        )
                    }
                }
            }
        }
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testScrollableWithDynamicToolbar() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        // Set active since setVerticalClipping call affects only for forground tab.
        mainSession.setActive(true)

        setupDocument(ROOT_100VH_HTML_PATH + "?event")

        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        assertResultDetail(
            ROOT_100VH_HTML_PATH,
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )

        // Prepare a resize event listener.
        val resizePromise = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                window.visualViewport.addEventListener('resize', () => {
                    resolve(true);
                }, { once: true });
            });
            """.trimIndent(),
        )

        // Hide the dynamic toolbar.
        sessionRule.display?.run { setVerticalClipping(-20) }

        // Wait a visualViewport resize event to make sure the toolbar change has been reflected.
        assertThat("resize", resizePromise.value as Boolean, equalTo(true))

        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))
        assertResultDetail(
            ROOT_100VH_HTML_PATH,
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_TOP,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testOverscrollBehaviorAuto() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }
        setupDocument(OVERSCROLL_BEHAVIOR_AUTO_HTML_PATH)

        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        assertResultDetail(
            "`overscroll-behavior: auto`",
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testOverscrollBehaviorAutoNone() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }
        setupDocument(OVERSCROLL_BEHAVIOR_AUTO_NONE_HTML_PATH)

        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        assertResultDetail(
            "`overscroll-behavior: auto, none`",
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            PanZoomController.OVERSCROLL_FLAG_HORIZONTAL,
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testOverscrollBehaviorNoneAuto() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }
        setupDocument(OVERSCROLL_BEHAVIOR_NONE_AUTO_HTML_PATH)

        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        assertResultDetail(
            "`overscroll-behavior: none, auto`",
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            PanZoomController.OVERSCROLL_FLAG_VERTICAL,
        )
    }

    // NOTE: This function requires #scroll element in the target document.
    private fun scrollToBottom() {
        // Prepare a scroll event listener.
        val scrollPromise = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                const scroll = document.getElementById('scroll');
                scroll.addEventListener('scroll', () => {
                    resolve(true);
                }, { once: true });
            });
            """.trimIndent(),
        )

        // Scroll to the bottom edge of the scroll container.
        mainSession.evaluateJS(
            """
            const scroll = document.getElementById('scroll');
            scroll.scrollTo(0, scroll.scrollHeight);
            """.trimIndent(),
        )
        assertThat("scroll", scrollPromise.value as Boolean, equalTo(true))
        mainSession.flushApzRepaints()
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testScrollHandoff() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }
        setupDocument(SCROLL_HANDOFF_HTML_PATH)

        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        // There is a child scroll container and its overscroll-behavior is `contain auto`
        assertResultDetail(
            "handoff",
            value,
            PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
            (PanZoomController.SCROLLABLE_FLAG_BOTTOM or PanZoomController.SCROLLABLE_FLAG_TOP),
            PanZoomController.OVERSCROLL_FLAG_VERTICAL,
        )

        // Scroll to the bottom edge
        scrollToBottom()

        value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        // Now the touch event should be handed to the root scroller.
        assertResultDetail(
            "handoff",
            value,
            PanZoomController.INPUT_RESULT_HANDLED,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testOverscrollBehaviorNoneOnNonRoot() {
        var files = arrayOf(
            OVERSCROLL_BEHAVIOR_NONE_NON_ROOT_HTML_PATH,
        )

        for (file in files) {
            setupDocument(file)

            var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

            assertResultDetail(
                "`overscroll-behavior: none` on non root scroll container",
                value,
                PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
                PanZoomController.SCROLLABLE_FLAG_BOTTOM,
                PanZoomController.OVERSCROLL_FLAG_NONE,
            )

            // Scroll to the bottom edge so that the container is no longer scrollable downwards.
            scrollToBottom()

            value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

            // The touch event should be handled in the scroll container content.
            assertResultDetail(
                "`overscroll-behavior: none` on non root scroll container",
                value,
                PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
                PanZoomController.SCROLLABLE_FLAG_TOP,
                PanZoomController.OVERSCROLL_FLAG_NONE,
            )
        }
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testOverscrollBehaviorNoneOnNonRootWithDynamicToolbar() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        var files = arrayOf(
            OVERSCROLL_BEHAVIOR_NONE_NON_ROOT_HTML_PATH,
        )

        for (file in files) {
            setupDocument(file)

            var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

            assertResultDetail(
                "`overscroll-behavior: none` on non root scroll container",
                value,
                PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
                PanZoomController.SCROLLABLE_FLAG_BOTTOM,
                PanZoomController.OVERSCROLL_FLAG_NONE,
            )

            // Scroll to the bottom edge so that the container is no longer scrollable downwards.
            scrollToBottom()

            value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

            // Now the touch event should be handed to the root scroller even if
            // the scroll container's `overscroll-behavior` is none to move
            // the dynamic toolbar.
            assertResultDetail(
                "`overscroll-behavior: none, none`",
                value,
                PanZoomController.INPUT_RESULT_HANDLED,
                PanZoomController.SCROLLABLE_FLAG_BOTTOM,
                (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
            )
        }
    }

    // Tests a situation where converting a scrollport size between CSS units and app units will
    // result different values, and the difference causes an issue that unscrollable documents
    // behave as if it's scrollable.
    //
    // Note about metrics that this test uses.
    // A basic here is that documents having no meta viewport tags are laid out on 980px width
    // canvas, the 980px is defined as "browser.viewport.desktopWidth".
    //
    // So, if the device screen size is (1080px, 2160px) then the document is scaled to
    // (1080 / 980) = 1.10204. Then if the dynamic toolbar height is 59px, the scaled document
    // height is (2160 - 59) / 1.10204 = 1906.46 (in CSS units).  It's converted and actually rounded
    // to 114388 (= 1906.46 * 60) in app units. And it's converted back to 1906.47 (114388 / 60) in
    // CSS units unfortunately.
    @WithDisplay(width = 1080, height = 2160)
    @Test
    fun testFractionalScrollPortSize() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "browser.viewport.desktopWidth" to 980,
            ),
        )
        sessionRule.display?.run { setDynamicToolbarMaxHeight(59) }

        setupDocument(NO_META_VIEWPORT_HTML_PATH)

        // Try to scroll down to see if the document is scrollable or not.
        var value = sessionRule.waitForResult(sendDownEvent(50f, 50f))

        assertResultDetail(
            "The document isn't not scrollable at all",
            value,
            PanZoomController.INPUT_RESULT_UNHANDLED,
            PanZoomController.SCROLLABLE_FLAG_NONE,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testPreventTouchMoveAfterLongTap() {
        sessionRule.display?.run { setDynamicToolbarMaxHeight(20) }

        setupDocument(ROOT_100VH_HTML_PATH)

        // Setup a touchmove event listener preventing scrolling.
        val touchmovePromise = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                window.addEventListener('touchmove', (e) => {
                    e.preventDefault();
                    resolve(true);
                }, { passive: false });
            });
            """.trimIndent(),
        )

        // Setup a contextmenu event.
        val contextmenuPromise = mainSession.evaluatePromiseJS(
            """
            new Promise(resolve => {
                window.addEventListener('contextmenu', (e) => {
                    e.preventDefault();
                    resolve(true);
                }, { once: true });
            });
            """.trimIndent(),
        )

        // Explicitly call `waitForRoundTrip()` to make sure the above event listeners
        // have set up in the content.
        mainSession.waitForRoundTrip()

        mainSession.flushApzRepaints()

        val downTime = SystemClock.uptimeMillis()
        val down = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_DOWN,
            50f,
            50f,
            0,
        )
        val result = mainSession.panZoomController.onTouchEventForDetailResult(down)

        // Wait until a contextmenu event happens.
        assertThat("contextmenu", contextmenuPromise.value as Boolean, equalTo(true))

        // Start moving.
        val move = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_MOVE,
            50f,
            70f,
            0,
        )
        mainSession.panZoomController.onTouchEvent(move)

        assertThat("touchmove", touchmovePromise.value as Boolean, equalTo(true))

        val value = sessionRule.waitForResult(result)

        // The input result for the initial touch-start event should have been handled by
        // the content.
        assertResultDetail(
            ROOT_100VH_HTML_PATH,
            value,
            PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
            PanZoomController.SCROLLABLE_FLAG_BOTTOM,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )

        val up = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_UP,
            50f,
            70f,
            0,
        )

        mainSession.panZoomController.onTouchEvent(up)
    }

    @WithDisplay(width = 100, height = 100)
    @Test
    fun testTouchCancelBeforeFirstTouchMove() {
        setupDocument(ROOT_100VH_HTML_PATH)

        // Setup a touchmove event listener preventing scrolling.
        mainSession.evaluateJS(
            """
            window.addEventListener('touchmove', (e) => {
                e.preventDefault();
            }, { passive: false });
            """.trimIndent(),
        )

        // Explicitly call `waitForRoundTrip()` to make sure the above event listener
        // has been set up in the content.
        mainSession.waitForRoundTrip()

        mainSession.flushApzRepaints()

        // Send a touchstart. The result will not be produced yet because
        // we will wait for the first touchmove.
        val downTime = SystemClock.uptimeMillis()
        val down = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_DOWN,
            50f,
            50f,
            0,
        )
        val result = mainSession.panZoomController.onTouchEventForDetailResult(down)

        // Before any touchmove, send a touchcancel.
        val cancel = MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            MotionEvent.ACTION_CANCEL,
            50f,
            50f,
            0,
        )
        mainSession.panZoomController.onTouchEvent(cancel)

        // Check that the touchcancel results in the same response as if
        // the touchmove was prevented.
        val value = sessionRule.waitForResult(result)
        assertResultDetail(
            "testTouchCancelBeforeFirstTouchMove",
            value,
            PanZoomController.INPUT_RESULT_HANDLED_CONTENT,
            PanZoomController.SCROLLABLE_FLAG_NONE,
            (PanZoomController.OVERSCROLL_FLAG_HORIZONTAL or PanZoomController.OVERSCROLL_FLAG_VERTICAL),
        )
    }
}
