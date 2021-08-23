/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.Handler
import android.os.Looper
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.SessionState
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSession.ScrollDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.ProgressDelegate
import org.mozilla.geckoview.GeckoSession.HistoryDelegate
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.*
import org.mozilla.geckoview.test.util.UiThreadUtils

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Test for the GeckoSessionTestRule class, to ensure it properly sets up a session for
 * each test, and to ensure it can properly wait for and assert delegate
 * callbacks.
 */
@RunWith(AndroidJUnit4::class)
@MediumTest
class GeckoSessionTestRuleTest : BaseSessionTest(noErrorCollector = true) {

    @Test fun getSession() {
        assertThat("Can get session", sessionRule.session, notNullValue())
        assertThat("Session is open",
                   sessionRule.session.isOpen, equalTo(true))
    }

    @ClosedSessionAtStart
    @Test fun getSession_closedSession() {
        assertThat("Session is closed", sessionRule.session.isOpen, equalTo(false))
    }

    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"),
                  Setting(key = Setting.Key.DISPLAY_MODE, value = "DISPLAY_MODE_MINIMAL_UI"),
                  Setting(key = Setting.Key.ALLOW_JAVASCRIPT, value = "false"))
    @Setting(key = Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test fun settingsApplied() {
        assertThat("USE_PRIVATE_MODE should be set",
                   sessionRule.session.settings.usePrivateMode,
                   equalTo(true))
        assertThat("DISPLAY_MODE should be set",
                   sessionRule.session.settings.displayMode,
                   equalTo(GeckoSessionSettings.DISPLAY_MODE_MINIMAL_UI))
        assertThat("USE_TRACKING_PROTECTION should be set",
                   sessionRule.session.settings.useTrackingProtection,
                   equalTo(true))
        assertThat("ALLOW_JAVASCRIPT should be set",
                sessionRule.session.settings.allowJavascript,
                equalTo(false))
    }

    @Test(expected = UiThreadUtils.TimeoutException::class)
    @TimeoutMillis(2000)
    fun noPendingCallbacks() {
        // Make sure we don't have unexpected pending callbacks at the start of a test.
        sessionRule.waitUntilCalled(object : ProgressDelegate, HistoryDelegate {
            // There may be extraneous onSessionStateChange and onHistoryStateChange calls
            // after a test, so ignore the first received.
            @AssertCalled(count = 2)
            override fun onSessionStateChange(session: GeckoSession, state: SessionState) {
            }

            @AssertCalled(count = 2)
            override fun onHistoryStateChange(session: GeckoSession, historyList: HistoryDelegate.HistoryList) {
            }
        })
    }

    @NullDelegate.List(NullDelegate(ContentDelegate::class),
                       NullDelegate(NavigationDelegate::class))
    @NullDelegate(ScrollDelegate::class)
    @Test fun nullDelegate() {
        assertThat("Content delegate should be null",
                   sessionRule.session.contentDelegate, nullValue())
        assertThat("Navigation delegate should be null",
                   sessionRule.session.navigationDelegate, nullValue())
        assertThat("Scroll delegate should be null",
                   sessionRule.session.scrollDelegate, nullValue())

        assertThat("Progress delegate should not be null",
                   sessionRule.session.progressDelegate, notNullValue())
    }

    @NullDelegate(ProgressDelegate::class)
    @ClosedSessionAtStart
    @Test fun nullDelegate_closed() {
        assertThat("Progress delegate should be null",
                   sessionRule.session.progressDelegate, nullValue())
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(ProgressDelegate::class)
    @ClosedSessionAtStart
    fun nullDelegate_requireProgressOnOpen() {
        assertThat("Progress delegate should be null",
                   sessionRule.session.progressDelegate, nullValue())

        sessionRule.session.open()
    }

    @Test fun waitForPageStop() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun waitForPageStops() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(ProgressDelegate::class)
    @ClosedSessionAtStart
    fun waitForPageStops_throwOnNullDelegate() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.open(sessionRule.runtime) // Avoid waiting for initial load
        sessionRule.session.reload()
        sessionRule.session.waitForPageStops(2)
    }

    @Test fun waitUntilCalled_anyInterfaceMethod() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(ProgressDelegate::class)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: ProgressDelegate.SecurityInformation) {
                counter++
            }

            override fun onProgressChange(session: GeckoSession, progress: Int) {
                counter++
            }

            override fun onSessionStateChange(session: GeckoSession, state: SessionState) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun waitUntilCalled_specificInterfaceMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(ProgressDelegate::class,
                                     "onPageStart", "onPageStop")

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test(expected = AssertionError::class)
    fun waitUntilCalled_throwOnNotGeckoSessionInterface() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(CharSequence::class)
    }

    fun waitUntilCalled_notThrowOnCallbackInterface() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(ProgressDelegate::class)
    }

    @NullDelegate(ScrollDelegate::class)
    @Test fun waitUntilCalled_notThrowOnNonNullDelegateMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(ProgressDelegate::class, "onPageStop")
    }

    @Test fun waitUntilCalled_anyObjectMethod() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        var counter = 0

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: ProgressDelegate.SecurityInformation) {
                counter++
            }

            override fun onProgressChange(session: GeckoSession, progress: Int) {
                counter++
            }

            override fun onSessionStateChange(session: GeckoSession, state: SessionState) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun waitUntilCalled_specificObjectMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        var counter = 0

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(ScrollDelegate::class)
    fun waitUntilCalled_throwOnNullDelegateObject() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(object : ScrollDelegate {
            @AssertCalled
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @NullDelegate(ScrollDelegate::class)
    @Test fun waitUntilCalled_notThrowOnNonNullDelegateObject() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun waitUntilCalled_multipleCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()

        var counter = 0

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 2)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(4))
    }

    @Test fun waitUntilCalled_currentCall() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()

        var counter = 0

        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 2, order = [1, 2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertThat("Method info should be valid", info, notNullValue())
                assertThat("Counter should be correct",
                           info.counter, equalTo(forEachCall(1, 2)))
                assertThat("Order should equal counter",
                           info.order, equalTo(info.counter))
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test(expected = IllegalStateException::class)
    fun waitUntilCalled_passThroughExceptions() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })
    }

    @Test fun waitUntilCalled_zeroCount() {
        // Support having @AssertCalled(count = 0) annotations for waitUntilCalled calls.
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : ProgressDelegate, ScrollDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }

            @AssertCalled(count = 0)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_anyMethod() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnAnyMethodNotCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ScrollDelegate {})
    }

    @Test fun forCallbacksDuringWait_specificMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun forCallbacksDuringWait_specificMethodMultipleTimes() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(4))
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnSpecificMethodNotCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ScrollDelegate {
            @AssertCalled
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_specificCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 2)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(4))
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_specificOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(order = [2])
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = [1])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_multipleOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(order = [1, 3, 1])
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = [2, 4, 1])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongMultipleOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(order = [1, 2, 1])
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = [3, 4, 1])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_notCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ScrollDelegate {
            @AssertCalled(false)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnCallingNoCall() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_zeroCountEqualsNotCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ScrollDelegate {
            @AssertCalled(count = 0)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnCallingZeroCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 0)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_limitedToLastWait() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.session.reload()
        sessionRule.session.reload()

        // Wait for Gecko to finish all loads.
        Thread.sleep(100)

        sessionRule.waitForPageStop() // Wait for loadUri.
        sessionRule.waitForPageStop() // Wait for first reload.

        var counter = 0

        // assert should only apply to callbacks within range (loadUri, first reload].
        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun forCallbacksDuringWait_currentCall() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertThat("Method info should be valid", info, notNullValue())
                assertThat("Counter should be correct",
                           info.counter, equalTo(1))
                assertThat("Order should equal counter",
                           info.order, equalTo(0))
            }
        })
    }

    @Test(expected = IllegalStateException::class)
    fun forCallbacksDuringWait_passThroughExceptions() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(ScrollDelegate::class)
    fun forCallbacksDuringWait_throwOnAnyNullDelegate() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : NavigationDelegate, ScrollDelegate {})
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(ScrollDelegate::class)
    fun forCallbacksDuringWait_throwOnSpecificNullDelegate() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : ScrollDelegate {
            @AssertCalled
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @NullDelegate(ScrollDelegate::class)
    @Test fun forCallbacksDuringWait_notThrowOnNonNullDelegate() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun getCurrentCall_throwOnNoCurrentCall() {
        sessionRule.currentCall
    }

    @Test fun delegateUntilTestEnd() {
        var counter = 0

        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun delegateUntilTestEnd_notCalled() {
        sessionRule.delegateUntilTestEnd(object : ScrollDelegate {
            @AssertCalled(false)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnNotCalled() {
        sessionRule.delegateUntilTestEnd(object : ScrollDelegate {
            @AssertCalled(count = 1)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
        sessionRule.performTestEndCheck()
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnCallingNoCall() {
        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnWrongOrder() {
        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [2])
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(count = 1, order = [1])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test fun delegateUntilTestEnd_currentCall() {
        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertThat("Method info should be valid", info, notNullValue())
                assertThat("Counter should be correct",
                           info.counter, equalTo(1))
                assertThat("Order should equal counter",
                           info.order, equalTo(0))
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test fun delegateDuringNextWait() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        var counter = 0

        sessionRule.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("Should have delegated", counter, equalTo(2))

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        assertThat("Delegate should be cleared", counter, equalTo(2))
    }

    @Test(expected = AssertionError::class)
    fun delegateDuringNextWait_throwOnNotCalled() {
        sessionRule.delegateDuringNextWait(object : ScrollDelegate {
            @AssertCalled(count = 1)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    fun delegateDuringNextWait_throwOnNotCalledAtTestEnd() {
        sessionRule.delegateDuringNextWait(object : ScrollDelegate {
            @AssertCalled(count = 1)
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
        sessionRule.performTestEndCheck()
    }

    @Test fun delegateDuringNextWait_hasPrecedence() {
        var testCounter = 0
        var waitCounter = 0

        sessionRule.delegateUntilTestEnd(object : ProgressDelegate,
                                                  NavigationDelegate {
            @AssertCalled(count = 1, order = [2])
            override fun onPageStart(session: GeckoSession, url: String) {
                testCounter++
            }

            @AssertCalled(count = 1, order = [4])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                testCounter++
            }

            @AssertCalled(count = 2, order = [1, 3])
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                testCounter++
            }

            @AssertCalled(count = 2, order = [1, 3])
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                testCounter++
            }
        })

        sessionRule.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onPageStart(session: GeckoSession, url: String) {
                waitCounter++
            }

            @AssertCalled(count = 1, order = [2])
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                waitCounter++
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("Text delegate should be overridden",
                   testCounter, equalTo(2))
        assertThat("Wait delegate should be used", waitCounter, equalTo(2))

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        assertThat("Test delegate should be used", testCounter, equalTo(6))
        assertThat("Wait delegate should be cleared", waitCounter, equalTo(2))
    }

    @Test(expected = IllegalStateException::class)
    fun delegateDuringNextWait_passThroughExceptions() {
        sessionRule.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(NavigationDelegate::class)
    fun delegateDuringNextWait_throwOnNullDelegate() {
        sessionRule.session.delegateDuringNextWait(object : NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String?) {
            }
        })
    }

    @Test fun wrapSession() {
        val session = sessionRule.wrapSession(
          GeckoSession(sessionRule.session.settings))
        sessionRule.openSession(session)
        session.reload()
        session.waitForPageStop()
    }

    @Test fun createOpenSession() {
        val newSession = sessionRule.createOpenSession()
        assertThat("Can create session", newSession, notNullValue())
        assertThat("New session is open", newSession.isOpen, equalTo(true))
        assertThat("New session has same settings",
                   newSession.settings, equalTo(sessionRule.session.settings))
    }

    @Test fun createOpenSession_withSettings() {
        val settings = GeckoSessionSettings.Builder(sessionRule.session.settings)
                .usePrivateMode(true)
                .build()

        val newSession = sessionRule.createOpenSession(settings)
        assertThat("New session has same settings", newSession.settings, equalTo(settings))
    }

    @Test fun createOpenSession_canInterleaveOtherCalls() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        val newSession = sessionRule.createOpenSession()
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        newSession.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun createClosedSession() {
        val newSession = sessionRule.createClosedSession()
        assertThat("Can create session", newSession, notNullValue())
        assertThat("New session is open", newSession.isOpen, equalTo(false))
        assertThat("New session has same settings",
                   newSession.settings, equalTo(sessionRule.session.settings))
    }

    @Test fun createClosedSession_withSettings() {
        val settings = GeckoSessionSettings.Builder(sessionRule.session.settings).usePrivateMode(true).build()

        val newSession = sessionRule.createClosedSession(settings)
        assertThat("New session has same settings", newSession.settings, equalTo(settings))
    }

    @Test(expected = UiThreadUtils.TimeoutException::class)
    @TimeoutMillis(2000)
    @ClosedSessionAtStart
    fun noPendingCallbacks_withSpecificSession() {
        sessionRule.createOpenSession()
        // Make sure we don't have unexpected pending callbacks after opening a session.
        sessionRule.waitUntilCalled(object : HistoryDelegate, ProgressDelegate {
            // There may be extraneous onSessionStateChange and onHistoryStateChange calls
            // after a test, so ignore the first received.
            @AssertCalled(count = 2)
            override fun onSessionStateChange(session: GeckoSession, state: SessionState) {
            }

            @AssertCalled(count = 2)
            override fun onHistoryStateChange(session: GeckoSession, historyList: HistoryDelegate.HistoryList) {
            }
        })
    }

    @Test fun waitForPageStop_withSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()
    }

    @Test fun waitForPageStop_withAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    fun waitForPageStop_throwOnNotWrapped() {
        GeckoSession(sessionRule.session.settings).waitForPageStop()
    }

    @Test fun waitForPageStops_withSpecificSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.reload()
        newSession.waitForPageStops(2)
    }

    @Test fun waitForPageStops_withAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)
    }

    @Test fun waitForPageStops_acrossSessionCreation() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        val session = sessionRule.createOpenSession()
        sessionRule.session.reload()
        session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(3)
    }

    @Test fun waitUntilCalled_interfaceWithSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitUntilCalled(ProgressDelegate::class, "onPageStop")
    }

    @Test fun waitUntilCalled_interfaceWithAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(ProgressDelegate::class, "onPageStop")
    }

    @Test fun waitUntilCalled_callbackWithSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun waitUntilCalled_callbackWithAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : ProgressDelegate {
            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_withSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        var counter = 0

        newSession.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun forCallbacksDuringWait_withAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun forCallbacksDuringWait_limitedToLastSessionWait() {
        val newSession = sessionRule.createOpenSession()

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        // forCallbacksDuringWait calls strictly apply to the last wait, session-specific or not.
        var counter = 0

        sessionRule.session.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.forCallbacksDuringWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun delegateUntilTestEnd_withSpecificSession() {
        val newSession = sessionRule.createOpenSession()

        var counter = 0

        newSession.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun delegateUntilTestEnd_withAllSessions() {
        var counter = 0

        sessionRule.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun delegateDuringNextWait_hasPrecedenceWithSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        var counter = 0

        newSession.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.delegateUntilTestEnd(object : ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun delegateDuringNextWait_specificSessionOverridesAll() {
        val newSession = sessionRule.createOpenSession()
        var counter = 0

        newSession.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.delegateDuringNextWait(object : ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @WithDisplay(width = 100, height = 100)
    @Test fun synthesizeTap() {
        sessionRule.session.loadTestPath(CLICK_TO_RELOAD_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.synthesizeTap(50, 50)
        sessionRule.session.waitForPageStop()
    }

    @Test fun evaluateExtensionJS() {
        assertThat("JS string result should be correct",
                sessionRule.evaluateExtensionJS("return 'foo';") as String, equalTo("foo"))

        assertThat("JS number result should be correct",
                sessionRule.evaluateExtensionJS("return 1+1;") as Double, equalTo(2.0))

        assertThat("JS boolean result should be correct",
                sessionRule.evaluateExtensionJS("return !0;") as Boolean, equalTo(true))

        val expected = JSONObject("{bar:42,baz:true,foo:'bar'}")
        val actual = sessionRule.evaluateExtensionJS("return {foo:'bar',bar:42,baz:true};") as JSONObject
        for (key in expected.keys()) {
            assertThat("JS object result should be correct",
                    actual.get(key), equalTo(expected.get(key)))
        }

        assertThat("JS array result should be correct",
                sessionRule.evaluateExtensionJS("return [1,2,3];") as JSONArray,
                equalTo(JSONArray("[1,2,3]")))

        assertThat("Can access extension APIS",
                sessionRule.evaluateExtensionJS("return !!browser.runtime;") as Boolean,
                equalTo(true))

        assertThat("Can access extension APIS",
                sessionRule.evaluateExtensionJS("""
                    return true;
                    // Comments at the end are allowed""".trimIndent()) as Boolean,
                equalTo(true))

        try {
            sessionRule.evaluateExtensionJS("test({ what")
            assertThat("Should fail", true, equalTo(false))
        } catch (e: RejectedPromiseException) {
            assertThat("Syntax errors are reported",
                    e.message,  containsString("SyntaxError"))
        }
    }

    @Test fun evaluateJS() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH);
        sessionRule.session.waitForPageStop();

        assertThat("JS string result should be correct",
                   sessionRule.session.evaluateJS("'foo'") as String, equalTo("foo"))

        assertThat("JS number result should be correct",
                   sessionRule.session.evaluateJS("1+1") as Double, equalTo(2.0))

        assertThat("JS boolean result should be correct",
                   sessionRule.session.evaluateJS("!0") as Boolean, equalTo(true))

        val expected = JSONObject("{bar:42,baz:true,foo:'bar'}")
        val actual = sessionRule.session.evaluateJS("({foo:'bar',bar:42,baz:true})") as JSONObject
        for (key in expected.keys()) {
            assertThat("JS object result should be correct",
                    actual.get(key), equalTo(expected.get(key)))
        }

        assertThat("JS array result should be correct",
                   sessionRule.session.evaluateJS("[1,2,3]") as JSONArray,
                   equalTo(JSONArray("[1,2,3]")))

        assertThat("JS DOM object result should be correct",
                   sessionRule.session.evaluateJS("document.body.tagName") as String,
                   equalTo("BODY"))
    }

    @Test fun evaluateJS_windowObject() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        assertThat("JS DOM window result should be correct",
                   (sessionRule.session.evaluateJS("window.location.pathname")) as String,
                   equalTo(HELLO_HTML_PATH))
    }

    @Test fun evaluateJS_multipleSessions() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.evaluateJS("this.foo = 42")
        assertThat("Variable should be set",
                   sessionRule.session.evaluateJS("this.foo") as Double, equalTo(42.0))

        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitForPageStop()

        val result = newSession.evaluateJS("this.foo")
        assertThat("New session should have separate JS context",
                   result, nullValue())
    }

    @Test fun evaluateJS_supportPromises() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        assertThat("Can get resolved promise",
            sessionRule.session.evaluatePromiseJS(
                    "new Promise(resolve => resolve('foo'))").value as String,
            equalTo("foo"));

        val promise = sessionRule.session.evaluatePromiseJS(
                "new Promise(r => window.resolve = r)")

        sessionRule.session.evaluateJS("window.resolve('bar')")

        assertThat("Can wait for promise to resolve",
                promise.value as String, equalTo("bar"))
    }

    @Test(expected = RejectedPromiseException::class)
    fun evaluateJS_throwOnRejectedPromise() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        sessionRule.session.evaluatePromiseJS("Promise.reject('foo')").value
    }

    @Test fun evaluateJS_notBlockMainThread() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        // Test that we can still receive delegate callbacks during evaluateJS,
        // by calling alert(), which blocks until prompt delegate is called.
        assertThat("JS blocking result should be correct",
                   sessionRule.session.evaluateJS("alert(); 'foo'") as String,
                   equalTo("foo"))
    }

    @TimeoutMillis(1000)
    @Test(expected = UiThreadUtils.TimeoutException::class)
    fun evaluateJS_canTimeout() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        sessionRule.session.delegateUntilTestEnd(object : PromptDelegate {
            override fun onAlertPrompt(session: GeckoSession, prompt: PromptDelegate.AlertPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                // Return a GeckoResult that we will never complete, so it hangs.
                val res = GeckoResult<PromptDelegate.PromptResponse>()
                return res
            }
        })
        sessionRule.session.evaluateJS("alert()")
    }

    @Test(expected = RuntimeException::class)
    fun evaluateJS_throwOnJSException() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        sessionRule.session.evaluateJS("throw Error()")
    }

    @Test(expected = RuntimeException::class)
    fun evaluateJS_throwOnSyntaxError() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        sessionRule.session.evaluateJS("<{[")
    }

    @Test(expected = RuntimeException::class)
    fun evaluateJS_throwOnChromeAccess() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()
        sessionRule.session.evaluateJS("ChromeUtils")
    }

    @Test fun getPrefs_undefinedPrefReturnsNull() {
        assertThat("Undefined pref should have null value",
                   sessionRule.getPrefs("invalid.pref")[0], equalTo(JSONObject.NULL))
    }

    @Test fun setPrefsUntilTestEnd() {
        sessionRule.setPrefsUntilTestEnd(mapOf(
                "test.pref.bool" to true,
                "test.pref.int" to 1,
                "test.pref.foo" to "foo"))

        var prefs = sessionRule.getPrefs(
                "test.pref.bool",
                "test.pref.int",
                "test.pref.foo",
                "test.pref.bar")

        assertThat("Prefs should be set", prefs[0] as Boolean, equalTo(true))
        assertThat("Prefs should be set", prefs[1] as Int, equalTo(1))
        assertThat("Prefs should be set", prefs[2] as String, equalTo("foo"))
        assertThat("Prefs should be set", prefs[3], equalTo(JSONObject.NULL))

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "test.pref.foo" to "bar",
                "test.pref.bar" to "baz"))

        prefs = sessionRule.getPrefs(
                        "test.pref.bool",
                        "test.pref.int",
                        "test.pref.foo",
                        "test.pref.bar")

        assertThat("New prefs should be set", prefs[0] as Boolean, equalTo(true))
        assertThat("New prefs should be set", prefs[1] as Int, equalTo(1))
        assertThat("New prefs should be set", prefs[2] as String, equalTo("bar"))
        assertThat("New prefs should be set", prefs[3] as String, equalTo("baz"))
    }

    @Test fun setPrefsDuringNextWait() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.setPrefsDuringNextWait(mapOf(
                "test.pref.bool" to true,
                "test.pref.int" to 1,
                "test.pref.foo" to "foo"))

        var prefs = sessionRule.getPrefs(
                        "test.pref.bool",
                        "test.pref.int",
                        "test.pref.foo")

        assertThat("Prefs should be set before wait", prefs[0] as Boolean, equalTo(true))
        assertThat("Prefs should be set before wait", prefs[1] as Int, equalTo(1))
        assertThat("Prefs should be set before wait", prefs[2] as String, equalTo("foo"))

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        prefs = sessionRule.getPrefs(
                "test.pref.bool",
                "test.pref.int",
                "test.pref.foo")

        assertThat("Prefs should be cleared after wait", prefs[0], equalTo(JSONObject.NULL))
        assertThat("Prefs should be cleared after wait", prefs[1], equalTo(JSONObject.NULL))
        assertThat("Prefs should be cleared after wait", prefs[2], equalTo(JSONObject.NULL))
    }

    @Test fun setPrefsDuringNextWait_hasPrecedence() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.setPrefsUntilTestEnd(mapOf(
                "test.pref.int" to 1,
                "test.pref.foo" to "foo"))

        sessionRule.setPrefsDuringNextWait(mapOf(
                "test.pref.foo" to "bar",
                "test.pref.bar" to "baz"))

        var prefs = sessionRule.getPrefs(
                "test.pref.int",
                "test.pref.foo",
                "test.pref.bar")

        assertThat("Prefs should be overridden", prefs[0] as Int, equalTo(1))
        assertThat("Prefs should be overridden", prefs[1] as String, equalTo("bar"))
        assertThat("Prefs should be overridden", prefs[2] as String, equalTo("baz"))

        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        prefs = sessionRule.getPrefs(
                        "test.pref.int",
                        "test.pref.foo",
                        "test.pref.bar")

        assertThat("Overriden prefs should be restored", prefs[0] as Int, equalTo(1))
        assertThat("Overriden prefs should be restored", prefs[1] as String, equalTo("foo"))
        assertThat("Overriden prefs should be restored", prefs[2], equalTo(JSONObject.NULL))
    }

    @Test fun waitForJS() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("waitForJS should return correct result",
                   sessionRule.session.waitForJS("alert(), 'foo'") as String,
                   equalTo("foo"))

        sessionRule.session.forCallbacksDuringWait(object : PromptDelegate {
            @AssertCalled(count = 1)
            override fun onAlertPrompt(session: GeckoSession, prompt: PromptDelegate.AlertPrompt): GeckoResult<PromptDelegate.PromptResponse>? {
                return null;
            }
        })
    }

    @Test fun waitForJS_resolvePromise() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
        assertThat("waitForJS should wait for promises",
                   sessionRule.session.waitForJS("Promise.resolve('foo')") as String,
                   equalTo("foo"))
    }

    @Test fun waitForJS_delegateDuringWait() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var count = 0
        sessionRule.session.delegateDuringNextWait(object : PromptDelegate {
            override fun onAlertPrompt(session: GeckoSession, prompt: PromptDelegate.AlertPrompt): GeckoResult<PromptDelegate.PromptResponse> {
                count++
                return GeckoResult.fromValue(prompt.dismiss())
            }
        })

        sessionRule.session.waitForJS("alert()")
        sessionRule.session.waitForJS("alert()")

        // The delegate set through delegateDuringNextWait
        // should have been cleared after the first wait.
        assertThat("Delegate should only run once", count, equalTo(1))
    }

    private interface TestDelegate {
        fun onDelegate(foo: String, bar: String): Int
    }

    @Test fun addExternalDelegateUntilTestEnd() {
        lateinit var delegate: TestDelegate

        sessionRule.addExternalDelegateUntilTestEnd(
                TestDelegate::class, { newDelegate -> delegate = newDelegate }, { },
                object : TestDelegate {
            @AssertCalled(count = 1)
            override fun onDelegate(foo: String, bar: String): Int {
                assertThat("First argument should be correct", foo, equalTo("foo"))
                assertThat("Second argument should be correct", bar, equalTo("bar"))
                return 42
            }
        })

        assertThat("Delegate should be registered", delegate, notNullValue())
        assertThat("Delegate return value should be correct",
                   delegate.onDelegate("foo", "bar"), equalTo(42))
        sessionRule.performTestEndCheck()
    }

    @Test(expected = AssertionError::class)
    fun addExternalDelegateUntilTestEnd_throwOnNotCalled() {
        sessionRule.addExternalDelegateUntilTestEnd(TestDelegate::class, { }, { },
                                                    object : TestDelegate {
            @AssertCalled(count = 1)
            override fun onDelegate(foo: String, bar: String): Int {
                return 42
            }
        })
        sessionRule.performTestEndCheck()
    }

    @Test fun addExternalDelegateDuringNextWait() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var delegate: Runnable? = null

        sessionRule.addExternalDelegateDuringNextWait(Runnable::class,
                                                      { newDelegate -> delegate = newDelegate },
                                                      { delegate = null }, Runnable { })

        assertThat("Delegate should be registered", delegate, notNullValue())
        delegate?.run()

        mainSession.reload()
        mainSession.waitForPageStop()
        mainSession.forCallbacksDuringWait(Runnable @AssertCalled(count = 1) {})

        assertThat("Delegate should be unregistered after wait", delegate, nullValue())
    }

    @Test fun addExternalDelegateDuringNextWait_hasPrecedence() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var delegate: TestDelegate? = null
        val register = { newDelegate: TestDelegate -> delegate = newDelegate }
        val unregister = { _: TestDelegate -> delegate = null }

        sessionRule.addExternalDelegateDuringNextWait(TestDelegate::class, register, unregister,
                object : TestDelegate {
                    @AssertCalled(count = 1)
                    override fun onDelegate(foo: String, bar: String): Int {
                        return 24
                    }
                })

        sessionRule.addExternalDelegateUntilTestEnd(TestDelegate::class, register, unregister,
                object : TestDelegate {
                    @AssertCalled(count = 1)
                    override fun onDelegate(foo: String, bar: String): Int {
                        return 42
                    }
                })

        assertThat("Wait delegate should be registered", delegate, notNullValue())
        assertThat("Wait delegate return value should be correct",
                delegate?.onDelegate("", ""), equalTo(24))

        mainSession.reload()
        mainSession.waitForPageStop()

        assertThat("Test delegate should still be registered", delegate, notNullValue())
        assertThat("Test delegate return value should be correct",
                delegate?.onDelegate("", ""), equalTo(42))
        sessionRule.performTestEndCheck()
    }

    @IgnoreCrash
    @Test fun contentCrashIgnored() {
        // TODO: Bug 1673953
        assumeThat(sessionRule.env.isFission, equalTo(false))

        // TODO: bug 1710940
        assumeThat(sessionRule.env.isIsolatedProcess, equalTo(false))

        mainSession.loadUri(CONTENT_CRASH_URL)
        mainSession.waitUntilCalled(object : ContentDelegate {
            @AssertCalled(count = 1)
            override fun onCrash(session: GeckoSession) = Unit
        })
    }

    @Test(expected = ChildCrashedException::class)
    fun contentCrashFails() {
        assumeThat(sessionRule.env.shouldShutdownOnCrash(), equalTo(false))

        sessionRule.session.loadUri(CONTENT_CRASH_URL)
        sessionRule.waitForPageStop()
    }

    @Test fun waitForResult() {
        val handler = Handler(Looper.getMainLooper())
        val result = object : GeckoResult<Int>() {
            init {
                handler.postDelayed({
                    complete(42)
                }, 100)
            }
        }

        val value = sessionRule.waitForResult(result)
        assertThat("Value should match", value, equalTo(42))
    }

    @Test(expected = IllegalStateException::class)
    fun waitForResultExceptionally() {
        val handler = Handler(Looper.getMainLooper())
        val result = object : GeckoResult<Int>() {
            init {
                handler.postDelayed({
                    completeExceptionally(IllegalStateException("boom"))
                }, 100)
            }
        }

        sessionRule.waitForResult(result)
    }
}
