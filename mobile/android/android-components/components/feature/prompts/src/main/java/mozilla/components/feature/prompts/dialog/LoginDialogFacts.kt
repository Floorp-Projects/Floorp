/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [LoginDialogFragment]
 */
class LoginDialogFacts {
    /**
     * Items that specify how the [LoginDialogFragment] was interacted with
     */
    object Items {
        const val DISPLAY = "display"
        const val SAVE = "save"
        const val NEVER_SAVE = "never_save"
        const val CANCEL = "cancel"
    }
}

private fun emitLoginDialogFacts(
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

internal fun emitDisplayFact() = emitLoginDialogFacts(Action.CLICK, LoginDialogFacts.Items.DISPLAY)
internal fun emitNeverSaveFact() = emitLoginDialogFacts(Action.CLICK, LoginDialogFacts.Items.NEVER_SAVE)
internal fun emitSaveFact() = emitLoginDialogFacts(Action.CLICK, LoginDialogFacts.Items.SAVE)
internal fun emitCancelFact() = emitLoginDialogFacts(Action.CLICK, LoginDialogFacts.Items.CANCEL)
