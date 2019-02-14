package org.mozilla.geckoview.test

import org.mozilla.geckoview.ScreenLength
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.PanZoomController

@RunWith(AndroidJUnit4::class)
@MediumTest
class PanZoomControllerTest : BaseSessionTest() {
    private val errorEpsilon = 3.0
    @WithDevToolsAPI
    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollBy() {
        sessionRule.session.loadTestPath(SCROLL_TEST_PATH)
        sessionRule.waitForPageStop()
        val vh = sessionRule.evaluateJS(mainSession, "window.innerHeight") as Double
        assertThat("Viewport height is not zero", vh, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollBy(ScreenLength.zero(), ScreenLength.fromViewportHeight(1.0), PanZoomController.SCROLL_BEHAVIOR_AUTO)
        val scrollY = sessionRule.evaluateJS(mainSession, "window.scrollY") as Double
        assertThat("scrollBy should have scrolled along y axis one viewport", scrollY, closeTo(vh, errorEpsilon))

        sessionRule.session.loadTestPath(SCROLL_TEST_PATH)
        sessionRule.waitForPageStop()
        val vw = sessionRule.evaluateJS(mainSession, "window.innerWidth") as Double
        assertThat("Viewport width is not zero", vw, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollBy(ScreenLength.fromViewportWidth(1.0), ScreenLength.zero(), PanZoomController.SCROLL_BEHAVIOR_AUTO)
        val scrollX = sessionRule.evaluateJS(mainSession, "window.scrollX") as Double
        assertThat("scrollBy should have scrolled along x axis one viewport", scrollX, closeTo(vw, errorEpsilon))
    }

    @WithDevToolsAPI
    @WithDisplay(width = 100, height = 100)
    @Test
    fun scrollTo() {
        sessionRule.session.loadTestPath(SCROLL_TEST_PATH)
        sessionRule.waitForPageStop()
        val vh = sessionRule.evaluateJS(mainSession, "window.innerHeight") as Double
        val scrollHeight = sessionRule.evaluateJS(mainSession, "window.document.body.scrollHeight") as Double
        assertThat("Viewport height is not zero", vh, greaterThan(0.0))
        assertThat("scrollHeight height is not zero", scrollHeight, greaterThan(0.0))
        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.bottom(), PanZoomController.SCROLL_BEHAVIOR_AUTO)
        var scrollY = sessionRule.evaluateJS(mainSession, "window.scrollY") as Double
        assertThat("scrollTo should have scrolled to bottom", scrollY, closeTo(scrollHeight - vh, 3.0))

        sessionRule.session.panZoomController.scrollTo(ScreenLength.zero(), ScreenLength.top(), PanZoomController.SCROLL_BEHAVIOR_AUTO)
        scrollY = sessionRule.evaluateJS(mainSession, "window.scrollY") as Double
        assertThat("scrollTo should have scrolled to top", scrollY, closeTo(0.0, errorEpsilon))

        val vw = sessionRule.evaluateJS(mainSession, "window.innerWidth") as Double
        val scrollWidth = sessionRule.evaluateJS(mainSession, "window.document.body.scrollWidth") as Double

        sessionRule.session.panZoomController.scrollTo(ScreenLength.fromViewportWidth(1.0), ScreenLength.zero(), PanZoomController.SCROLL_BEHAVIOR_AUTO)
        val scrollX = sessionRule.evaluateJS(mainSession, "window.scrollX") as Double
        assertThat("scrollTo should have scrolled to right", scrollX, closeTo(scrollWidth - vw, errorEpsilon))
    }
}