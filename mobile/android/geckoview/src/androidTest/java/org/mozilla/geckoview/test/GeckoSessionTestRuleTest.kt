/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ClosedSessionAtStart
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.TimeoutException
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.TimeoutMillis
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Ignore
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

    @Test(expected = TimeoutException::class)
    @TimeoutMillis(1000)
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

    @NullDelegate.List(NullDelegate(GeckoSession.ContentDelegate::class),
                       NullDelegate(Callbacks.NavigationDelegate::class))
    @NullDelegate(Callbacks.ScrollDelegate::class)
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

    @NullDelegate(GeckoSession.ProgressDelegate::class)
    @ClosedSessionAtStart
    @Test fun nullDelegate_closed() {
        assertThat("Progress delegate should be null",
                   sessionRule.session.progressDelegate, nullValue())
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ProgressDelegate::class)
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

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ProgressDelegate::class)
    @ClosedSessionAtStart
    fun waitForPageStops_throwOnNullDelegate() {
        sessionRule.session.open(sessionRule.runtime) // Avoid waiting for initial load
        sessionRule.session.reload()
        sessionRule.session.waitForPageStops(2)
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

    fun waitUntilCalled_notThrowOnCallbackInterface() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(Callbacks.ProgressDelegate::class)
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ScrollDelegate::class)
    fun waitUntilCalled_throwOnNullDelegateInterface() {
        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(Callbacks.All::class)
    }

    @NullDelegate(GeckoSession.ScrollDelegate::class)
    @Test fun waitUntilCalled_notThrowOnNonNullDelegateMethod() {
        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(Callbacks.All::class, "onPageStop")
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

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ScrollDelegate::class)
    fun waitUntilCalled_throwOnNullDelegateObject() {
        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(object : Callbacks.All {
            @AssertCalled
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @NullDelegate(GeckoSession.ScrollDelegate::class)
    @Test fun waitUntilCalled_notThrowOnNonNullDelegateObject() {
        sessionRule.session.reload()
        sessionRule.session.waitUntilCalled(object : Callbacks.All {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
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
        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

    @Test fun forCallbacksDuringWait_zeroCountEqualsNotCalled() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                GeckoSession.ScrollDelegate @AssertCalled(count = 0) { _, _, _ -> })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnCallingZeroCount() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

    @Test(expected = IllegalStateException::class)
    fun forCallbacksDuringWait_passThroughExceptions() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ScrollDelegate::class)
    fun forCallbacksDuringWait_throwOnAnyNullDelegate() {
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.All {})
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.ScrollDelegate::class)
    fun forCallbacksDuringWait_throwOnSpecificNullDelegate() {
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.All {
            @AssertCalled
            override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
            }
        })
    }

    @NullDelegate(GeckoSession.ScrollDelegate::class)
    @Test fun forCallbacksDuringWait_notThrowOnNonNullDelegate() {
        sessionRule.session.reload()
        sessionRule.session.waitForPageStop()

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.All {
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

        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
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

        sessionRule.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
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
        sessionRule.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                throw IllegalStateException()
            }
        })

        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStop()
    }

    @Test(expected = AssertionError::class)
    @NullDelegate(GeckoSession.NavigationDelegate::class)
    fun delegateDuringNextWait_throwOnNullDelegate() {
        sessionRule.session.delegateDuringNextWait(object : Callbacks.NavigationDelegate {
            override fun onLocationChange(session: GeckoSession, url: String) {
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
        val settings = GeckoSessionSettings(sessionRule.session.settings)
        settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true)

        val newSession = sessionRule.createOpenSession(settings)
        assertThat("New session has same settings", newSession.settings, equalTo(settings))
    }

    @Test fun createOpenSession_canInterleaveOtherCalls() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)

        val newSession = sessionRule.createOpenSession()
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(2)

        newSession.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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
        val settings = GeckoSessionSettings(sessionRule.session.settings)
        settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true)

        val newSession = sessionRule.createClosedSession(settings)
        assertThat("New session has same settings", newSession.settings, equalTo(settings))
    }

    @Test(expected = TimeoutException::class)
    @TimeoutMillis(1000)
    @ClosedSessionAtStart
    fun noPendingCallbacks_withSpecificSession() {
        sessionRule.createOpenSession()
        // Make sure we don't have unexpected pending callbacks after opening a session.
        sessionRule.waitUntilCalled(object : Callbacks.All {})
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
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        val session = sessionRule.createOpenSession()
        sessionRule.session.reload()
        session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitForPageStops(3)
    }

    @Test fun waitUntilCalled_interfaceWithSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitUntilCalled(Callbacks.ProgressDelegate::class, "onPageStop")
    }

    @Test fun waitUntilCalled_interfaceWithAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(Callbacks.ProgressDelegate::class, "onPageStop")
    }

    @Test fun waitUntilCalled_callbackWithSpecificSession() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        newSession.waitUntilCalled(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun waitUntilCalled_callbackWithAllSessions() {
        val newSession = sessionRule.createOpenSession()
        newSession.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.waitUntilCalled(object : Callbacks.ProgressDelegate {
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

        newSession.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        sessionRule.session.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressDelegate {
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

        newSession.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.session.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
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

        sessionRule.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
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
        var newSession = sessionRule.createOpenSession()
        var counter = 0

        newSession.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        newSession.delegateUntilTestEnd(object : Callbacks.ProgressDelegate {
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
        var newSession = sessionRule.createOpenSession()
        var counter = 0

        newSession.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        sessionRule.delegateDuringNextWait(object : Callbacks.ProgressDelegate {
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

    @WithDisplay(width = 10, height = 10)
    @Test fun synthesizeTap() {
        // synthesizeTap is unreliable under e10s.
        assumeThat(sessionRule.env.isMultiprocess, equalTo(false))

        sessionRule.session.loadTestPath(CLICK_TO_RELOAD_HTML_PATH)
        sessionRule.session.waitForPageStop()

        sessionRule.session.synthesizeTap(5, 5)
        sessionRule.session.waitForPageStop()
    }

    @WithDevToolsAPI
    @Ignore
    @Test fun evaluateJS() {
        assertThat("JS string result should be correct",
                   sessionRule.session.evaluateJS("'foo'") as String, equalTo("foo"))

        assertThat("JS number result should be correct",
                   sessionRule.session.evaluateJS("1+1") as Double, equalTo(2.0))

        assertThat("JS boolean result should be correct",
                   sessionRule.session.evaluateJS("!0") as Boolean, equalTo(true))

        assertThat("JS object result should be correct",
                   sessionRule.session.evaluateJS("({foo:'bar',bar:42,baz:true})").asJSMap(),
                   equalTo(mapOf("foo" to "bar", "bar" to 42.0, "baz" to true)))

        assertThat("JS array result should be correct",
                   sessionRule.session.evaluateJS("[1,2,3]").asJSList(),
                   equalTo(listOf(1.0, 2.0, 3.0)))

        assertThat("JS DOM object result should be correct",
                   sessionRule.session.evaluateJS("document.body").asJSMap(),
                   hasEntry("tagName", "BODY"))

        assertThat("JS DOM array result should be correct",
                   sessionRule.session.evaluateJS("document.childNodes").asJSList<Any>(),
                   not(empty()))
    }

    @WithDevToolsAPI
    @Ignore
    @Test fun evaluateJS_windowObject() {
        sessionRule.session.loadTestPath(HELLO_HTML_PATH)
        sessionRule.session.waitForPageStop()

        // Make sure we can access large objects like "window", which can strain our RDP connection.
        // Also make sure we can dynamically access sub-objects like Location.
        assertThat("JS DOM window result should be correct",
                   (sessionRule.session.evaluateJS("window")
                           dot "location"
                           dot "pathname") as String,
                   equalTo(HELLO_HTML_PATH))
    }

    @WithDevToolsAPI
    @Ignore
    @Test fun evaluateJS_multipleSessions() {
        val newSession = sessionRule.createOpenSession()

        sessionRule.session.evaluateJS("this.foo = 42")
        assertThat("Variable should be set",
                   sessionRule.session.evaluateJS("this.foo") as Double, equalTo(42.0))

        assertThat("New session should have separate JS context",
                   newSession.evaluateJS("this.foo"), nullValue())
    }

    @WithDevToolsAPI
    @Ignore
    @Test fun evaluateJS_jsToString() {
        val obj = sessionRule.session.evaluateJS("({foo:'bar'})")
        assertThat("JS object toString should follow lazy evaluation",
                   obj.toString(), equalTo("[Object]"))

        assertThat("JS object should be correct",
                   (obj dot "foo") as String, equalTo("bar"))
        assertThat("JS object toString should be expanded after evaluation",
                   obj.toString(), equalTo("[Object]{foo=bar}"))

        val array = sessionRule.session.evaluateJS("['foo','bar']")
        assertThat("JS array toString should follow lazy evaluation",
                   array.toString(), equalTo("[Array(2)]"))

        assertThat("JS array should be correct",
                   (array dot 0) as String, equalTo("foo"))
        assertThat("JS array toString should be expanded after evaluation",
                   array.toString(), equalTo("[Array(2)][foo, bar]"))
    }

    @Test(expected = AssertionError::class)
    fun evaluateJS_throwOnNotWithDevTools() {
        sessionRule.session.evaluateJS("0")
    }

    @WithDevToolsAPI
    @Ignore
    @Test(expected = RuntimeException::class)
    fun evaluateJS_throwOnJSException() {
        sessionRule.session.evaluateJS("throw Error()")
    }

    @WithDevToolsAPI
    @Ignore
    @Test(expected = RuntimeException::class)
    fun evaluateJS_throwOnSyntaxError() {
        sessionRule.session.evaluateJS("<{[")
    }
}
