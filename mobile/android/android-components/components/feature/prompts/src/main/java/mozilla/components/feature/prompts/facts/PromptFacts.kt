/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for different events related to the prompt feature.
 */
class PromptFacts {
    /**
     * Different events emitted by prompts.
     */
    object Items {
        const val PROMPT = "PROMPT"
    }
}

private fun emitFact(
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

internal fun emitPromptDisplayedFact(promptName: String) {
    emitFact(
        Action.DISPLAY,
        PromptFacts.Items.PROMPT,
        value = promptName,
    )
}

internal fun emitPromptDismissedFact(promptName: String) {
    emitFact(
        Action.CANCEL,
        PromptFacts.Items.PROMPT,
        value = promptName,
    )
}

internal fun emitPromptConfirmedFact(promptName: String) {
    emitFact(
        Action.CONFIRM,
        PromptFacts.Items.PROMPT,
        value = promptName,
    )
}
