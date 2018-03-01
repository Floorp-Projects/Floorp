/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class NavigationDelegateTest : BaseSessionTest() {

    @Test fun load() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int): Boolean {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URI should not be null", uri, notNullValue())
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should not be null", where, notNullValue())
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return false
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("URL should not be null", url, notNullValue())
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Session should not be null", session, notNullValue())
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String,
                                      response: GeckoSession.Response<GeckoSession>) {
            }
        })
    }

    @Test fun reload() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int): Boolean {
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return false
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String,
                                      response: GeckoSession.Response<GeckoSession>) {
            }
        })
    }

    @Test fun goBackAndForward() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }
        })

        sessionRule.session.goBack()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int): Boolean {
                assertThat("URI should match", uri, endsWith(HELLO_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return false
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Cannot go back", canGoBack, equalTo(false))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Can go forward", canGoForward, equalTo(true))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String,
                                      response: GeckoSession.Response<GeckoSession>) {
            }
        })

        sessionRule.session.goForward()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int): Boolean {
                assertThat("URI should match", uri, endsWith(HELLO2_HTML_PATH))
                assertThat("Where should match", where,
                           equalTo(GeckoSession.NavigationDelegate.TARGET_WINDOW_CURRENT))
                return false
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onLocationChange(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                assertThat("Can go back", canGoBack, equalTo(true))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                assertThat("Cannot go forward", canGoForward, equalTo(false))
            }

            @AssertCalled(false)
            override fun onNewSession(session: GeckoSession, uri: String,
                                      response: GeckoSession.Response<GeckoSession>) {
            }
        })
    }

    @Test fun onLoadUri_returnTrueCancelsLoad() {
        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 2)
            override fun onLoadRequest(session: GeckoSession, uri: String,
                                       where: Int): Boolean {
                return uri.endsWith(HELLO_HTML_PATH)
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO2_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                assertThat("URL should match", url, endsWith(HELLO2_HTML_PATH))
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                assertThat("Load should succeed", success, equalTo(true))
            }
        })
    }

    @WithDisplay(width = 128, height = 128)
    @Test fun onNewSession_calledForNewWindow() {
        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String, response: GeckoSession.Response<GeckoSession>) {
                response.respond(null)
            }
        })

        sessionRule.session.synthesizeTap(5, 5)
        sessionRule.waitUntilCalled(GeckoSession.NavigationDelegate::class, "onNewSession")
    }

    @WithDisplay(width = 128, height = 128)
    @Test(expected = IllegalArgumentException::class)
    fun onNewSession_doesNotAllowOpened() {
        sessionRule.session.loadTestPath(NEW_SESSION_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            @AssertCalled(count = 1)
            override fun onNewSession(session: GeckoSession, uri: String, response: GeckoSession.Response<GeckoSession>) {
                var newSession = GeckoSession(session.settings)
                newSession.open()
                response.respond(newSession)
            }
        })

        sessionRule.session.synthesizeTap(5, 5)
        sessionRule.waitUntilCalled(GeckoSession.NavigationDelegate::class, "onNewSession")
    }
}
