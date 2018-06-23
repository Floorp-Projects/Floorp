/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.SystemClock
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.MediumTest
import android.view.KeyEvent

import org.hamcrest.Matchers.*
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.junit.runners.Parameterized.Parameter

@MediumTest
@RunWith(Parameterized::class)
@WithDevToolsAPI
class TextInputDelegateTest : BaseSessionTest() {
    // "parameters" needs to be a static field, so it has to be in a companion object.
    companion object {
        @get:Parameterized.Parameters(name = "{0}")
        @JvmStatic
        val parameters: List<Array<out Any>> = listOf(
                arrayOf("#input"),
                arrayOf("#textarea"),
                arrayOf("#contenteditable"),
                arrayOf("#designmode"))
    }

    @field:Parameter(0) @JvmField var id: String = ""

    private fun pressKey(keyCode: Int) {
        // Create a Promise to listen to the key event, and wait on it below.
        val promise = mainSession.evaluateJS(
                "new Promise(r => window.addEventListener('keyup', r, { once: true }))")
        val time = SystemClock.uptimeMillis()
        val keyEvent = KeyEvent(time, time, KeyEvent.ACTION_DOWN, keyCode, 0);
        mainSession.textInput.onKeyDown(keyCode, keyEvent)
        mainSession.textInput.onKeyUp(keyCode, KeyEvent.changeAction(keyEvent, KeyEvent.ACTION_UP))
        promise.asJSPromise().value
    }

    @Test fun restartInput() {
        // Check that restartInput is called on focus and blur.
        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        mainSession.evaluateJS("$('$id').focus()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }
        })

        mainSession.evaluateJS("$('$id').blur()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1)
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_BLUR))
            }

            // Also check that showSoftInput/hideSoftInput are not called before a user action.
            @AssertCalled(count = 0)
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    @Test fun restartInput_temporaryFocus() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Focus the input once here and once below, but we should only get a
        // single restartInput or showSoftInput call for the second focus.
        mainSession.evaluateJS("$('$id').focus(); $('$id').blur()")

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("$('$id').focus()")

        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct",
                           reason, equalTo(GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS))
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
                super.showSoftInput(session)
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
                super.hideSoftInput(session)
            }
        })
    }

    @Test fun restartInput_temporaryBlur() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)
        mainSession.evaluateJS("$('$id').focus()")
        mainSession.waitUntilCalled(GeckoSession.TextInputDelegate::class,
                                    "restartInput", "showSoftInput")

        // We should get a pair of restartInput calls for the blur/focus,
        // but only one showSoftInput call and no hideSoftInput call.
        mainSession.evaluateJS("$('$id').blur(); $('$id').focus()")

        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 2, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
                assertThat("Reason should be correct", reason, equalTo(forEachCall(
                        GeckoSession.TextInputDelegate.RESTART_REASON_BLUR,
                        GeckoSession.TextInputDelegate.RESTART_REASON_FOCUS)))
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }

    @Test fun showHideSoftInput() {
        // Our user action trick doesn't work for design-mode, so we can't test that here.
        assumeThat("Not in designmode", id, not(equalTo("#designmode")))

        mainSession.loadTestPath(INPUTS_PATH)
        mainSession.waitForPageStop()

        // Simulate a user action so we're allowed to show/hide the keyboard.
        pressKey(KeyEvent.KEYCODE_CTRL_LEFT)

        mainSession.evaluateJS("$('$id').focus()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
            }

            @AssertCalled(count = 1, order = [2])
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 0)
            override fun hideSoftInput(session: GeckoSession) {
            }
        })

        mainSession.evaluateJS("$('$id').blur()")
        mainSession.waitUntilCalled(object : Callbacks.TextInputDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun restartInput(session: GeckoSession, reason: Int) {
            }

            @AssertCalled(count = 0)
            override fun showSoftInput(session: GeckoSession) {
            }

            @AssertCalled(count = 1, order = [2])
            override fun hideSoftInput(session: GeckoSession) {
            }
        })
    }
}
