/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupActivity
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.APP_LINK
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.HOMESCREEN
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.UNKNOWN
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupState.Cold
import org.mozilla.fenix.perf.StartupTimelineStateMachine.getNextState

class StartupTimelineStateMachineTest {

    @Test
    fun `GIVEN state cold-unknown WHEN home activity is first shown THEN we are in cold-homescreen state`() {
        val actual = getNextState(Cold(UNKNOWN), StartupActivity.HOME)
        assertEquals(Cold(HOMESCREEN), actual)
    }

    @Test
    fun `GIVEN state cold-unknown WHEN intent receiver activity is first shown THEN we are in cold-app-link state`() {
        val actual = getNextState(Cold(UNKNOWN), StartupActivity.INTENT_RECEIVER)
        assertEquals(Cold(APP_LINK), actual)
    }

    @Test
    fun `GIVEN state cold + known destination WHEN any activity is passed in THEN we remain in the same state`() {
        val knownDestinations = StartupDestination.values().filter { it != UNKNOWN }
        val allActivities = StartupActivity.values()

        knownDestinations.forEach { destination ->
            val initial = Cold(destination)
            allActivities.forEach { activity ->
                val actual = getNextState(initial, activity)
                assertEquals("$destination $activity", initial, actual)
            }
        }
    }
}
