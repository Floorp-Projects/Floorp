/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.feature.prompts.concept.SelectablePromptView

/**
 * Delegate for credit card picker and related callbacks
 */
interface CreditCardDelegate {
    /**
     * The [SelectablePromptView] used for [CreditCardPicker] to display a
     * selectable prompt list of credit cards.
     */
    val creditCardPickerView: SelectablePromptView<CreditCardEntry>?
        get() = null

    /**
     * Callback invoked when a user selects "Manage credit cards"
     * from the select credit card prompt.
     */
    val onManageCreditCards: () -> Unit
        get() = {}

    /**
     * Callback invoked when a user selects a credit card option
     * from the select credit card prompt
     */
    val onSelectCreditCard: () -> Unit
        get() = {}
}
