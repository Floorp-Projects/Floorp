/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.APP_LINK
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.HOMESCREEN
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.UNKNOWN

/**
 * A state machine representing application startup for use with [StartupTimeline]. Android
 * application startup is complex so it's helpful to make all of our expected states explicit, e.g.
 * with a state machine, which helps check our assumptions. Unfortunately, because this state machine
 * is not used by the framework to determine possible startup scenarios, this is duplicating the
 * startup logic and is thus extremely fragile (especially because most devs won't know about this
 * class when they change the startup flow!). We may be able to mitigate this with assertions.
 *
 * To devs changing this class: by design as a state machine, this class should never hold any state
 * and should be 100% unit tested to validate assumptions.
 */
object StartupTimelineStateMachine {

    /**
     * The states the application passes through during startup. We define these states to help us
     * better understand Android startup. Note that these states are not 100% correlated to the
     * cold/warm/hot states Google Play Vitals uses.
     */
    sealed class StartupState {
        /** The state when the application is starting up but is not in memory. */
        data class Cold(val destination: StartupDestination) : StartupState()
    }

    /**
     * The final screen the user will see during startup.
     */
    enum class StartupDestination {
        HOMESCREEN,
        APP_LINK,
        UNKNOWN,
    }

    /**
     * A list of Activities supported by the app.
     */
    enum class StartupActivity {
        HOME,
        INTENT_RECEIVER,
    }

    /**
     * Given the current state and any arguments, returns the next state of the state machine.
     */
    fun getNextState(currentState: StartupState, startingActivity: StartupActivity): StartupState {
        return when (currentState) {
            is StartupState.Cold -> nextStateIsCold(currentState, startingActivity)
        }
    }

    private fun nextStateIsCold(currentState: StartupState.Cold, startingActivity: StartupActivity): StartupState {
        return when (currentState.destination) {
            UNKNOWN -> when (startingActivity) {
                StartupActivity.HOME -> StartupState.Cold(HOMESCREEN)
                StartupActivity.INTENT_RECEIVER -> StartupState.Cold(APP_LINK)
            }

            // We haven't defined the state machine after these states yet so we return the current state.
            else -> currentState
        }
    }
}
