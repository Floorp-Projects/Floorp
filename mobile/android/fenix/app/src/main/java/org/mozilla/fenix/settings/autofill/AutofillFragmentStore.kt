/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.autofill

import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [AutofillFragmentState] and applying [AutofillAction]s.
 */
class AutofillFragmentStore(initialState: AutofillFragmentState) :
    Store<AutofillFragmentState, AutofillAction>(
        initialState,
        ::autofillFragmentStateReducer,
    )

/**
 * The state used for managing autofill data.
 *
 * @property addresses The list of [Address]es to display in the address list.
 * @property creditCards The list of [CreditCard]s to display in the credit card list.
 * @property isLoading True if the addresses or credit cards are still being loaded from storage,
 * otherwise false.
 */
data class AutofillFragmentState(
    val addresses: List<Address> = emptyList(),
    val creditCards: List<CreditCard> = emptyList(),
    val isLoading: Boolean = true,
) : State

/**
 * Actions to dispatch through the [AutofillFragmentStore] to modify the [AutofillFragmentState]
 * through the [autofillFragmentStateReducer].
 */
sealed class AutofillAction : Action {
    /**
     * Updates the list of addresses with the provided [addresses].
     *
     * @property addresses The list of [Address]es to display in the address list.
     */
    data class UpdateAddresses(val addresses: List<Address>) : AutofillAction()

    /**
     * Updates the list of credit cards with the provided [creditCards].
     *
     * @property creditCards The list of [CreditCard]s to display in the credit card list.
     */
    data class UpdateCreditCards(val creditCards: List<CreditCard>) : AutofillAction()
}

/**
 * Reduces the autofill state from the current state with the provided [action] to be performed.
 *
 * @param state The current autofill state.
 * @param action The action to be performed on the state.
 * @return the new [AutofillFragmentState] with the [action] executed.
 */
private fun autofillFragmentStateReducer(
    state: AutofillFragmentState,
    action: AutofillAction,
): AutofillFragmentState {
    return when (action) {
        is AutofillAction.UpdateAddresses -> {
            state.copy(
                addresses = action.addresses,
                isLoading = false,
            )
        }
        is AutofillAction.UpdateCreditCards -> {
            state.copy(
                creditCards = action.creditCards,
                isLoading = false,
            )
        }
    }
}
