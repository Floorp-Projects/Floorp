/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import mozilla.components.concept.storage.CreditCard
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [CreditCardsListState] and applying [CreditCardsAction]s.
 */
class CreditCardsFragmentStore(initialState: CreditCardsListState) :
    Store<CreditCardsListState, CreditCardsAction>(
        initialState, ::creditCardsFragmentStateReducer
    )

/**
 * The state for [CreditCardsManagementFragment].
 *
 * @property creditCards The list of [CreditCard]s to display in the credit card list.
 * @property isLoading True if the credit cards are still being loaded from storage,
 * otherwise false.
 */
data class CreditCardsListState(
    val creditCards: List<CreditCard>,
    val isLoading: Boolean = true
) : State

/**
 * Actions to dispatch through the [CreditCardsFragmentStore] to modify the [CreditCardsListState]
 * through the [creditCardsFragmentStateReducer].
 */
sealed class CreditCardsAction : Action {
    /**
     * Updates the list of credit cards with the provided [creditCards].
     *
     * @param creditCards The list of [CreditCard]s to display in the credit card list.
     */
    data class UpdateCreditCards(val creditCards: List<CreditCard>) : CreditCardsAction()
}

/**
 * Reduces the credit cards state from the current state with the provided [action] to be performed.
 *
 * @param state The current credit cards state.
 * @param action The action to be performed on the state.
 * @return the new [CreditCardsListState] with the [action] executed.
 */
private fun creditCardsFragmentStateReducer(
    state: CreditCardsListState,
    action: CreditCardsAction
): CreditCardsListState {
    return when (action) {
        is CreditCardsAction.UpdateCreditCards -> {
            state.copy(
                creditCards = action.creditCards,
                isLoading = false
            )
        }
    }
}
