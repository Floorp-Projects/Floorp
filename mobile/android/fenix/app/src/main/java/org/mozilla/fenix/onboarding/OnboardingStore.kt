/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [OnboardingFragmentState] and applying [OnboardingAction]s.
 */
class OnboardingStore(
    initialState: OnboardingFragmentState = OnboardingFragmentState(),
) : Store<OnboardingFragmentState, OnboardingAction>(
    initialState = initialState,
    reducer = ::onboardingFragmentStateReducer,
)

/**
 * The state used for managing [OnboardingFragment].
 *
 * @property onboardingState Describes the onboarding account state.
 */
data class OnboardingFragmentState(
    val onboardingState: OnboardingState = OnboardingState.SignedOutNoAutoSignIn,
) : State

/**
 * Actions to dispatch through the [OnboardingStore] to modify the [OnboardingFragmentState]
 * through the [onboardingFragmentStateReducer].
 */
sealed class OnboardingAction : Action {
    /**
     * Updates the onboarding account state.
     *
     * @param state The onboarding account state to display.
     */
    data class UpdateState(val state: OnboardingState) : OnboardingAction()
}

/**
 * Reduces the onboarding state from the current state with the provided [action] to be performed.
 *
 * @param state The current onboarding state.
 * @param action The action to be performed on the state.
 * @return the new [OnboardingState] with the [action] executed.
 */
private fun onboardingFragmentStateReducer(
    state: OnboardingFragmentState,
    action: OnboardingAction,
): OnboardingFragmentState {
    return when (action) {
        is OnboardingAction.UpdateState -> state.copy(onboardingState = action.state)
    }
}
