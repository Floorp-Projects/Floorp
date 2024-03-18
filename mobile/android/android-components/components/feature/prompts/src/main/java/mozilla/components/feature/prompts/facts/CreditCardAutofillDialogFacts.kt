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
        const val AUTOFILL_CREDIT_CARD_FORM_DETECTED = "autofill_credit_card_form_detected"
        const val AUTOFILL_CREDIT_CARD_SUCCESS = "autofill_credit_card_success"
        const val AUTOFILL_CREDIT_CARD_PROMPT_SHOWN = "autofill_credit_card_prompt_shown"
        const val AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED = "autofill_credit_card_prompt_expanded"
        const val AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED = "autofill_credit_card_prompt_dismissed"
        const val AUTOFILL_CREDIT_CARD_CREATED = "autofill_credit_card_created"
        const val AUTOFILL_CREDIT_CARD_UPDATED = "autofill_credit_card_updated"
        const val AUTOFILL_CREDIT_CARD_SAVE_PROMPT_SHOWN = "autofill_credit_card_save_prompt_shown"
    }
}

private fun emitCreditCardAutofillDialogFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_PROMPTS,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitSuccessfulCreditCardAutofillFormDetectedFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_FORM_DETECTED,
    )
}

internal fun emitSuccessfulCreditCardAutofillSuccessFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS,
    )
}

internal fun emitCreditCardAutofillShownFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN,
    )
}

internal fun emitCreditCardAutofillExpandedFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED,
    )
}

internal fun emitCreditCardAutofillDismissedFact() {
    emitCreditCardAutofillDialogFact(
        Action.INTERACTION,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED,
    )
}

internal fun emitCreditCardAutofillCreatedFact() {
    emitCreditCardAutofillDialogFact(
        Action.CONFIRM,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_CREATED,
    )
}

internal fun emitCreditCardAutofillUpdatedFact() {
    emitCreditCardAutofillDialogFact(
        Action.CONFIRM,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_UPDATED,
    )
}

internal fun emitCreditCardSaveShownFact() {
    emitCreditCardAutofillDialogFact(
        Action.DISPLAY,
        CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SAVE_PROMPT_SHOWN,
    )
}
