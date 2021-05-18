/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Autofill prompt feature for credit cards.
 */
class CreditCardAutofillDialogFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val AUTOFILL_CREDIT_CARD_SUCCESS = "autofill_credit_card_success"
    }
}

private fun emitCreditCardAutofillDialogFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null
) {
    Fact(
        Component.FEATURE_PROMPTS,
        action,
        item,
        value,
        metadata
    ).collect()
}

internal fun emitSuccessfulCreditCardAutofillFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS
    )
}
