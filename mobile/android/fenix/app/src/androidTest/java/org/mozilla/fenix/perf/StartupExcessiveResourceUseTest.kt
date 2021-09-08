/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.children
import androidx.recyclerview.widget.RecyclerView
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.HomeActivityTestRule

// BEFORE INCREASING THESE VALUES, PLEASE CONSULT WITH THE PERF TEAM.
private const val EXPECTED_SUPPRESSION_COUNT = 19
@Suppress("TopLevelPropertyNaming") // it's silly this would have a different naming convention b/c no const
private val EXPECTED_RUNBLOCKING_RANGE = 0..1 // CI has +1 counts compared to local runs: increment these together
private const val EXPECTED_RECYCLER_VIEW_CONSTRAINT_LAYOUT_CHILDREN = 4
private const val EXPECTED_NUMBER_OF_INFLATION = 12

private val failureMsgStrictMode = getErrorMessage(
    shortName = "StrictMode suppression",
    implications = "suppressing a StrictMode violation can introduce performance regressions?"
)

private val failureMsgRunBlocking = getErrorMessage(
    shortName = "runBlockingIncrement",
    implications = "using runBlocking may block the main thread and have other negative performance implications?"
)

private val failureMsgRecyclerViewConstraintLayoutChildren = getErrorMessage(
    shortName = "ConstraintLayout being a common direct descendant of a RecyclerView",
    implications = "ConstraintLayouts are slow to inflate and are primarily used to flatten deep " +
        "view hierarchies so can be under-performant as a common RecyclerView child?"
) + "Please note that we're not sure if this is a useful metric to assert: with your feedback, " +
    "we'll find out over time if it is or is not."

private val failureMsgNumberOfInflation = getErrorMessage(
    shortName = "Number of inflation on start up doesn't match expected count",
    implications = "The number of inflation can negatively impact start up time. Having more inflations" +
        "will most likely mean we're adding extra work on the UI thread."
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
 * The perf team is code owners for this package so they should be notified when the counts are modified.
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

        val rootView = activityTestRule.activity.findViewById<LinearLayout>(R.id.rootContainer)
        val actualRecyclerViewConstraintLayoutChildren = countRecyclerViewConstraintLayoutChildren(rootView, null)

        val actualNumberOfInflations = InflationCounter.inflationCount.get()

        assertEquals(failureMsgStrictMode, EXPECTED_SUPPRESSION_COUNT, actualSuppresionCount)
        assertTrue(failureMsgRunBlocking + "actual: $actualRunBlocking", actualRunBlocking in EXPECTED_RUNBLOCKING_RANGE)
        assertEquals(
            failureMsgRecyclerViewConstraintLayoutChildren,
            EXPECTED_RECYCLER_VIEW_CONSTRAINT_LAYOUT_CHILDREN,
            actualRecyclerViewConstraintLayoutChildren
        )
        assertEquals(failureMsgNumberOfInflation, EXPECTED_NUMBER_OF_INFLATION, actualNumberOfInflations)
    }
}

private fun countRecyclerViewConstraintLayoutChildren(view: View, parent: View?): Int {
    val viewValue = if (parent is RecyclerView && view is ConstraintLayout) {
        1
    } else {
        0
    }

    return if (view !is ViewGroup) {
        viewValue
    } else {
        viewValue + view.children.sumOf { countRecyclerViewConstraintLayoutChildren(it, view) }
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
