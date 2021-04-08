/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.perf.AppStartReasonProvider.StartReason
import org.mozilla.fenix.perf.StartupActivityLog.LogEntry

class StartupStateProviderTest {

    private lateinit var provider: StartupStateProvider
    @MockK private lateinit var startupActivityLog: StartupActivityLog
    @MockK private lateinit var startReasonProvider: AppStartReasonProvider

    private lateinit var logEntries: MutableList<LogEntry>

    private val homeActivityClass = HomeActivity::class.java
    private val irActivityClass = IntentReceiverActivity::class.java

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        provider = StartupStateProvider(startupActivityLog, startReasonProvider)

        logEntries = mutableListOf()
        every { startupActivityLog.log } returns logEntries

        every { startReasonProvider.reason } returns StartReason.ACTIVITY // default to minimize repetition.
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is cold start THEN cold start is true`() {
        forEachColdStartEntries { index ->
            assertTrue("$index", provider.isColdStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN we launched HA through a drawing IntentRA THEN start up is not cold`() {
        // These entries mimic observed behavior for local code changes.
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(irActivityClass),
            LogEntry.ActivityStarted(irActivityClass),
            LogEntry.AppStarted,
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.ActivityStopped(irActivityClass)
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN two HomeActivities are created THEN start up is not cold`() {
        // We're making an assumption about how this would work based on previous observed patterns.
        // AIUI, we should never have more than one HomeActivity.
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted,
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.ActivityStopped(homeActivityClass)
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity and we're truncating the log for optimization WHEN warm start THEN start up is not cold`() {
        // These entries are from observed behavior.
        logEntries.addAll(listOf(
            LogEntry.AppStopped,
            LogEntry.ActivityStopped(homeActivityClass),
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity and we're truncating the log for optimization WHEN hot start THEN start up is not cold`() {
        // These entries are from observed behavior.
        logEntries.addAll(listOf(
            LogEntry.AppStopped,
            LogEntry.ActivityStopped(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity and we're not truncating the log for optimization WHEN warm start THEN start up is not cold`() {
        // While the entries are from observed behavior, this log shouldn't occur in the wild due to
        // our log optimizations. However, just in case the behavior changes, we check for it.
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted,
            LogEntry.AppStopped,
            LogEntry.ActivityStopped(homeActivityClass),
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity and we're not truncating the log for optimization WHEN hot start THEN start up is not cold`() {
        // This shouldn't occur in the wild due to the optimization but, just in case the behavior changes,
        // we check for it.
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted,
            LogEntry.AppStopped,
            LogEntry.ActivityStopped(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN multiple activities are started but not stopped (maybe impossible) THEN start up is not cold`() {
        fun assertIsNotCold() { assertFalse(provider.isColdStartForStartedActivity(homeActivityClass)) }

        // Since we've never observed this, there are multiple ways the events could
        // theoretically be ordered: we try a few.
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(irActivityClass),
            LogEntry.ActivityStarted(irActivityClass),
            LogEntry.AppStarted,
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass)
        ))
        assertIsNotCold()

        logEntries.clear()
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(irActivityClass),
            LogEntry.ActivityStarted(irActivityClass),
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertIsNotCold()

        logEntries.clear()
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(irActivityClass),
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(irActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))
        assertIsNotCold()
    }

    @Test
    fun `GIVEN the app started for an activity WHEN an activity hasn't been created yet THEN start up is not cold`() {
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN an activity hasn't started yet THEN start up is not cold`() {
        logEntries.addAll(listOf(
            LogEntry.ActivityCreated(homeActivityClass)
        ))
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app did not start for an activity WHEN is cold is checked THEN it returns false`() {
        every { startReasonProvider.reason } returns StartReason.NON_ACTIVITY

        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))

        forEachColdStartEntries { index ->
            assertFalse("$index", provider.isColdStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app has been stopped WHEN is cold short circuit is called THEN it returns true`() {
        logEntries.add(LogEntry.AppStopped)
        assertTrue(provider.shouldShortCircuitColdStart())
    }

    @Test
    fun `GIVEN the app has not been stopped WHEN is cold short circuit is called THEN it returns false`() {
        assertFalse(provider.shouldShortCircuitColdStart())
    }

    private fun forEachColdStartEntries(block: (index: Int) -> Unit) {
        // These entries mimic observed behavior.
        //
        // MAIN: open HomeActivity directly.
        val coldStartEntries = listOf(listOf(
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted

            // VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
        ), listOf(
            LogEntry.ActivityCreated(irActivityClass),
            LogEntry.ActivityCreated(homeActivityClass),
            LogEntry.ActivityStarted(homeActivityClass),
            LogEntry.AppStarted
        ))

        forEachStartEntry(coldStartEntries, block)
    }

    private fun forEachStartEntry(entries: List<List<LogEntry>>, block: (index: Int) -> Unit) {
        entries.forEachIndexed { index, startEntry ->
            logEntries.clear()
            logEntries.addAll(startEntry)
            block(index)
        }
    }
}
