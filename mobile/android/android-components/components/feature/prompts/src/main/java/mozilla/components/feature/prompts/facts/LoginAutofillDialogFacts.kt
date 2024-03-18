/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Autofill prompt feature for logins.
 */
class LoginAutofillDialogFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val AUTOFILL_LOGIN_PROMPT_SHOWN = "autofill_login_prompt_shown"
        const val AUTOFILL_LOGIN_PROMPT_DISMISSED = "autofill_login_prompt_dismissed"
        const val AUTOFILL_LOGIN_PERFORMED = "autofill_login_performed"
    }
}

private fun emitLoginAutofillDialogFact(
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

internal fun emitLoginAutofillShownFact() {
    emitLoginAutofillDialogFact(
        Action.INTERACTION,
        LoginAutofillDialogFacts.Items.AUTOFILL_LOGIN_PROMPT_SHOWN,
    )
}

internal fun emitLoginAutofillPerformedFact() {
    emitLoginAutofillDialogFact(
        Action.INTERACTION,
        LoginAutofillDialogFacts.Items.AUTOFILL_LOGIN_PERFORMED,
    )
}

internal fun emitLoginAutofillDismissedFact() {
    emitLoginAutofillDialogFact(
        Action.INTERACTION,
        LoginAutofillDialogFacts.Items.AUTOFILL_LOGIN_PROMPT_DISMISSED,
    )
}
