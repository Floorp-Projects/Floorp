/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.TimeoutMillis
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.LargeTest
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4

import org.hamcrest.Matcher
import org.hamcrest.Matchers.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ErrorCollector
import org.junit.runner.RunWith

inline fun GeckoSession.loadTestPath(path: String) =
        this.loadUri(GeckoSessionTestRule.APK_URI_PREFIX + path.removePrefix("/"))

/**
 * Test for the GeckoSessionTestRule class, to ensure it properly sets up a session for
 * each test, and to ensure it can properly wait for and assert delegate
 * callbacks.
 */
@RunWith(AndroidJUnit4::class)
@MediumTest
class GeckoSessionTestRuleTest {
    companion object {
        const val HELLO_HTML_PATH = "assets/www/hello.html"
    }

    @get:Rule val sessionRule = GeckoSessionTestRule()

    @get:Rule val errors = ErrorCollector()
    fun <T> assertThat(reason: String, v: T, m: Matcher<T>) = errors.checkThat(reason, v, m)

    @Test fun getSession() {
        assertThat("Can get session", sessionRule.session, notNullValue())
        assertThat("Session is open",
                   sessionRule.session.isOpen, equalTo(true))
    }

    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"),
                  Setting(key = Setting.Key.DISPLAY_MODE, value = "DISPLAY_MODE_MINIMAL_UI"))
    @Setting(key = Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test fun settingsApplied() {
        assertThat("USE_PRIVATE_MODE should be set",
                   sessionRule.session.settings.getBoolean(
                           GeckoSessionSettings.USE_PRIVATE_MODE),
                   equalTo(true))
        assertThat("DISPLAY_MODE should be set",
                   sessionRule.session.settings.getInt(GeckoSessionSettings.DISPLAY_MODE),
                   equalTo(GeckoSessionSettings.DISPLAY_MODE_MINIMAL_UI))
        assertThat("USE_TRACKING_PROTECTION should be set",
                   sessionRule.session.settings.getBoolean(
                           GeckoSessionSettings.USE_TRACKING_PROTECTION),
                   equalTo(true))
    }

    @Test(expected = AssertionError::class)
    @TimeoutMillis(1000)
    @LargeTest
    fun noPendingCallbacks() {
        // Make sure we don't have unexpected pending callbacks at the start of a test.
        sessionRule.waitUntilCalled(object : Callbacks.All {})
    }

    @Test fun includesAllCallbacks() {
        for (ifce in GeckoSession::class.java.classes) {
            if (!ifce.isInterface || !ifce.simpleName.endsWith("Delegate")) {
                continue
            }
            assertThat("Callbacks.All should include interface " + ifce.simpleName,
                       ifce.isInstance(Callbacks.Default), equalTo(true))
        }
    }

    @Test fun waitForPageStop() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test(expected = AssertionError::class)
    fun waitForPageStop_throwOnChangedCallback() {
        sessionRule.session.progressDelegate = Callbacks.Default
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test fun waitForPageStops() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun waitUntilCalled_anyInterfaceMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(GeckoSession.ProgressDelegate::class)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun waitUntilCalled_specificInterfaceMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(GeckoSession.ProgressDelegate::class,
                                     "onPageStart", "onPageStop")

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

    @Test fun waitUntilCalled_anyObjectMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(1))
    }

    @Test fun waitUntilCalled_specificObjectMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
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

    @Test fun waitUntilCalled_multipleCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
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

        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 2, order = intArrayOf(1, 2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertThat("Method info should be valid", info, notNullValue())
                assertThat("Counter should be correct",
                           info.counter, isOneOf(1, 2))
                assertThat("Order should equal counter",
                           info.order, equalTo(info.counter))
                counter++
            }
        })

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun forCallbacksDuringWait_anyMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(GeckoSession.ScrollDelegate { _, _, _ -> })
    }

    @Test fun forCallbacksDuringWait_specificMethod() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(
                GeckoSession.ScrollDelegate @AssertCalled { _, _, _ -> })
    }

    @Test fun forCallbacksDuringWait_specificCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(order = intArrayOf(2))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_multipleOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(order = intArrayOf(1, 3, 1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(2, 4, 1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongMultipleOrder() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(order = intArrayOf(1, 2, 1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(3, 4, 1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_notCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                GeckoSession.ScrollDelegate @AssertCalled(false) { _, _, _ -> })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnCallingNoCall() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(false)
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
        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

    @Test(expected = AssertionError::class)
    fun getCurrentCall_throwOnNoCurrentCall() {
        sessionRule.currentCall
    }

    @Test fun delegateUntilTestEnd() {
        var counter = 0

        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        assertThat("Callback count should be correct", counter, equalTo(2))
    }

    @Test fun delegateUntilTestEnd_notCalled() {
        sessionRule.delegateUntilTestEnd(
                GeckoSession.ScrollDelegate @AssertCalled(false) { _, _, _ -> })
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnNotCalled() {
        sessionRule.delegateUntilTestEnd(
                GeckoSession.ScrollDelegate @AssertCalled(count = 1) { _, _, _ -> })
        sessionRule.performTestEndCheck()
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnCallingNoCall() {
        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    fun delegateUntilTestEnd_throwOnWrongOrder() {
        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test fun delegateUntilTestEnd_currentCall() {
        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
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
        var counter = 0

        sessionRule.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
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
        sessionRule.delegateDuringNextWait(
                GeckoSession.ScrollDelegate @AssertCalled(count = 1) { _, _, _ -> })
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    fun delegateDuringNextWait_throwOnNotCalledAtTestEnd() {
        sessionRule.delegateDuringNextWait(
                GeckoSession.ScrollDelegate @AssertCalled(count = 1) { _, _, _ -> })
        sessionRule.performTestEndCheck()
    }

    @Test fun delegateDuringNextWait_hasPrecedence() {
        var testCounter = 0
        var waitCounter = 0

        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate,
                                                  Callbacks.NavigationDelegate {
            @AssertCalled(count = 1, order = intArrayOf(2))
            override fun onPageStart(session: GeckoSession, url: String) {
                testCounter++
            }

            @AssertCalled(count = 1, order = intArrayOf(4))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                testCounter++
            }

            @AssertCalled(count = 2, order = intArrayOf(1, 3))
            override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                testCounter++
            }

            @AssertCalled(count = 2, order = intArrayOf(1, 3))
            override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                testCounter++
            }
        })

        sessionRule.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1, order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
                waitCounter++
            }

            @AssertCalled(count = 1, order = intArrayOf(2))
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
}