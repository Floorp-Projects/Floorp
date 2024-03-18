/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.perf.AppStartReasonProvider.StartReason
import org.mozilla.fenix.perf.StartupActivityLog.LogEntry
import org.mozilla.fenix.perf.StartupStateProvider.StartupState

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
    fun `GIVEN the app started for an activity WHEN warm start THEN cold start is false`() {
        forEachWarmStartEntries { index ->
            assertFalse("$index", provider.isColdStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN hot start THEN cold start is false`() {
        forEachHotStartEntries { index ->
            assertFalse("$index", provider.isColdStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is cold start THEN warm start is false`() {
        forEachColdStartEntries { index ->
            assertFalse("$index", provider.isWarmStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is warm start THEN warm start is true`() {
        forEachWarmStartEntries { index ->
            assertTrue("$index", provider.isWarmStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is hot start THEN warm start is false`() {
        forEachHotStartEntries { index ->
            assertFalse("$index", provider.isWarmStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is cold start THEN hot start is false`() {
        forEachColdStartEntries { index ->
            assertFalse("$index", provider.isHotStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is warm start THEN hot start is false`() {
        forEachWarmStartEntries { index ->
            assertFalse("$index", provider.isHotStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN is hot start THEN hot start is true`() {
        forEachHotStartEntries { index ->
            assertTrue("$index", provider.isHotStartForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `GIVEN the app started for an activity WHEN we launched HA through a drawing IntentRA THEN start up is not cold`() {
        // These entries mimic observed behavior for local code changes.
        logEntries.addAll(
            listOf(
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(irActivityClass),
                LogEntry.AppStarted,
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.ActivityStopped(irActivityClass),
            ),
        )
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN we launched HA through a drawing IntentRA THEN start up is not warm`() {
        // These entries mimic observed behavior for local code changes.
        logEntries.addAll(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(irActivityClass),
                LogEntry.AppStarted,
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.ActivityStopped(irActivityClass),
            ),
        )
        assertFalse(provider.isWarmStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN we launched HA through a drawing IntentRA THEN start up is not hot`() {
        // These entries mimic observed behavior for local code changes.
        logEntries.addAll(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(irActivityClass),
                LogEntry.AppStarted,
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.ActivityStopped(irActivityClass),
            ),
        )
        assertFalse(provider.isHotStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN two HomeActivities are created THEN start up is not cold`() {
        // We're making an assumption about how this would work based on previous observed patterns.
        // AIUI, we should never have more than one HomeActivity.
        logEntries.addAll(
            listOf(
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.ActivityStopped(homeActivityClass),
            ),
        )
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN an activity hasn't been created yet THEN start up is not cold`() {
        assertFalse(provider.isColdStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN an activity hasn't started yet THEN start up is not cold`() {
        logEntries.addAll(
            listOf(
                LogEntry.ActivityCreated(homeActivityClass),
            ),
        )
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

    @Test
    fun `GIVEN the app has not been stopped WHEN an activity has not been created THEN it's not a warm start`() {
        assertFalse(provider.isWarmStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app has been stopped WHEN an activity has not been created THEN it's not a warm start`() {
        logEntries.add(LogEntry.AppStopped)
        assertFalse(provider.isWarmStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app has been stopped WHEN an activity has not been started THEN it's not a warm start`() {
        logEntries.addAll(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityCreated(homeActivityClass),
            ),
        )
        assertFalse(provider.isWarmStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app has not been stopped WHEN an activity has not been created THEN it's not a hot start`() {
        assertFalse(provider.isHotStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app has been stopped WHEN an activity has not been created THEN it's not a hot start`() {
        logEntries.add(LogEntry.AppStopped)
        assertFalse(provider.isHotStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app has been stopped WHEN an activity has not been started THEN it's not a hot start`() {
        logEntries.addAll(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityCreated(homeActivityClass),
            ),
        )
        assertFalse(provider.isHotStartForStartedActivity(homeActivityClass))
    }

    @Test
    fun `GIVEN the app started for an activity WHEN it is a cold start THEN get startup state is cold`() {
        forEachColdStartEntries { index ->
            assertEquals("$index", StartupState.COLD, provider.getStartupStateForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `WHEN it is a warm start THEN get startup state is warm`() {
        forEachWarmStartEntries { index ->
            assertEquals("$index", StartupState.WARM, provider.getStartupStateForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `WHEN it is a hot start THEN get startup state is hot`() {
        forEachHotStartEntries { index ->
            assertEquals("$index", StartupState.HOT, provider.getStartupStateForStartedActivity(homeActivityClass))
        }
    }

    @Test
    fun `WHEN two activities are started THEN get startup state is unknown`() {
        logEntries.addAll(
            listOf(
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(irActivityClass),
                LogEntry.AppStarted,
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.ActivityStopped(irActivityClass),
            ),
        )

        assertEquals(StartupState.UNKNOWN, provider.getStartupStateForStartedActivity(homeActivityClass))
    }

    private fun forEachColdStartEntries(block: (index: Int) -> Unit) {
        // These entries mimic observed behavior.
        //
        // MAIN: open HomeActivity directly.
        val coldStartEntries = listOf(
            listOf(
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
            ),
            listOf(
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
            ),
        )

        forEachStartEntry(coldStartEntries, block)
    }

    private fun forEachWarmStartEntries(block: (index: Int) -> Unit) {
        // These entries mimic observed behavior. We test both truncated (i.e. the current behavior
        // with the optimization to prevent an infinite log) and untruncated (the behavior without
        // such an optimization).
        //
        // truncated MAIN: open HomeActivity directly.
        val warmStartEntries = listOf(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // untruncated MAIN: open HomeActivity directly.
            ),
            listOf(
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // truncated VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
            ),
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // untruncated VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
            ),
            listOf(
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
            ),
        )

        forEachStartEntry(warmStartEntries, block)
    }

    private fun forEachHotStartEntries(block: (index: Int) -> Unit) {
        // These entries mimic observed behavior. We test both truncated (i.e. the current behavior
        // with the optimization to prevent an infinite log) and untruncated (the behavior without
        // such an optimization).
        //
        // truncated MAIN: open HomeActivity directly.
        val hotStartEntries = listOf(
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // untruncated MAIN: open HomeActivity directly.
            ),
            listOf(
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // truncated VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
            ),
            listOf(
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,

                // untruncated VIEW: open non-drawing IntentReceiverActivity, then HomeActivity.
            ),
            listOf(
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityCreated(homeActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
                LogEntry.AppStopped,
                LogEntry.ActivityStopped(homeActivityClass),
                LogEntry.ActivityCreated(irActivityClass),
                LogEntry.ActivityStarted(homeActivityClass),
                LogEntry.AppStarted,
            ),
        )

        forEachStartEntry(hotStartEntries, block)
    }

    private fun forEachStartEntry(entries: List<List<LogEntry>>, block: (index: Int) -> Unit) {
        entries.forEachIndexed { index, startEntry ->
            logEntries.clear()
            logEntries.addAll(startEntry)
            block(index)
        }
    }
}
