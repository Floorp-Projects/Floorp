/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Autofill prompt feature for addresses.
 */
class AddressAutofillDialogFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val AUTOFILL_ADDRESS_FORM_DETECTED = "autofill_address_form_detected"
        const val AUTOFILL_ADDRESS_SUCCESS = "autofill_address_success"
        const val AUTOFILL_ADDRESS_PROMPT_SHOWN = "autofill_address_prompt_shown"
        const val AUTOFILL_ADDRESS_PROMPT_EXPANDED = "autofill_address_prompt_expanded"
        const val AUTOFILL_ADDRESS_PROMPT_DISMISSED = "autofill_address_prompt_dismissed"
    }
}

private fun emitAddressAutofillDialogFact(
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

internal fun emitSuccessfulAddressAutofillFormDetectedFact() {
    emitAddressAutofillDialogFact(
        Action.INTERACTION,
        AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_FORM_DETECTED,
    )
}

internal fun emitSuccessfulAddressAutofillSuccessFact() {
    emitAddressAutofillDialogFact(
        Action.INTERACTION,
        AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_SUCCESS,
    )
}

internal fun emitAddressAutofillShownFact() {
    emitAddressAutofillDialogFact(
        Action.INTERACTION,
        AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_SHOWN,
    )
}

internal fun emitAddressAutofillExpandedFact() {
    emitAddressAutofillDialogFact(
        Action.INTERACTION,
        AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_EXPANDED,
    )
}

internal fun emitAddressAutofillDismissedFact() {
    emitAddressAutofillDialogFact(
        Action.INTERACTION,
        AddressAutofillDialogFacts.Items.AUTOFILL_ADDRESS_PROMPT_DISMISSED,
    )
}
