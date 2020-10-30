/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.perf.RunBlockingCounter
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.perf.ComponentInitCount

// BEFORE INCREASING THESE VALUES, PLEASE CONSULT WITH THE PERF TEAM.
private const val EXPECTED_SUPPRESSION_COUNT = 11
private const val EXPECTED_RUNBLOCKING_COUNT = 2
private const val EXPECTED_COMPONENT_INIT_COUNT = 42

private val failureMsgStrictMode = getErrorMessage(
    shortName = "StrictMode suppression",
    implications = "suppressing a StrictMode violation can introduce performance regressions?"
)

private val failureMsgRunBlocking = getErrorMessage(
    shortName = "runBlockingIncrement",
    implications = "using runBlocking may block the main thread and have other negative performance implications?"
)

private val failureMsgComponentInit = getErrorMessage(
    shortName = "Component init",
    implications = "initializing new components on start up may be an indication that we're doing more work than necessary on start up?"
)

/**
 * A performance test to limit the number of StrictMode suppressions and number of runBlocking used
 * on startup.
 *
 * This test was written by the perf team.
 *
 * StrictMode detects main thread IO, which is often indicative of a performance issue.
 * It's easy to suppress StrictMode so we wrote a test to ensure we have a discussion
 * if the StrictMode count changes.
 *
 * RunBlocking is mostly used to return values to a thread from a coroutine. However, if that
 * coroutine takes too long, it can lead that thread to block every other operations.
 *
 * The perf team is code owners for this file so they should be notified when the count is modified.
 *
 * IF YOU UPDATE THE TEST NAME, UPDATE CODE OWNERS.
 */
class StartupExcessiveResourceUseTest {
    @get:Rule
    val activityTestRule = HomeActivityTestRule(skipOnboarding = true)

    private val uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

    @Test
    fun verifyRunBlockingAndStrictModeSuppresionCount() {
        uiDevice.waitForIdle() // wait for async UI to load.

        // This might cause intermittents: at an arbitrary point after start up (such as the visual
        // completeness queue), we might run code on the main thread that suppresses StrictMode,
        // causing this number to fluctuate depending on device speed. We'll deal with it if it occurs.
        val actualSuppresionCount = activityTestRule.activity.components.strictMode.suppressionCount.get().toInt()
        val actualRunBlocking = RunBlockingCounter.count.get()
        val actualComponentInitCount = ComponentInitCount.count.get()

        assertEquals(failureMsgStrictMode, EXPECTED_SUPPRESSION_COUNT, actualSuppresionCount)
        assertEquals(failureMsgRunBlocking, EXPECTED_RUNBLOCKING_COUNT, actualRunBlocking)
        assertEquals(failureMsgComponentInit, EXPECTED_COMPONENT_INIT_COUNT, actualComponentInitCount)
    }
}

private fun getErrorMessage(shortName: String, implications: String) = """$shortName count does not match expected count.

    If this PR removed a $shortName call, great! Please decrease the count.

    Did this PR add or call code that increases the $shortName count?
    Did you know that $implications
    Please do your best to implement a solution without adding $shortName calls.
    Please consult the perf team if you have questions or believe that having this call
    is the optimal solution.

"""
