/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu.facts

import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [ContextMenuFeature]
 */
class ContextMenuFacts {
    /**
     * Items that specify which portion of the [ContextMenuFeature] was interacted with
     */
    object Items {
        const val ITEM = "item"
        const val TEXT_SELECTION_OPTION = "textSelectionOption"
    }
}

private fun emitContextMenuFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_CONTEXTMENU,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitClickFact(candidate: ContextMenuCandidate) {
    val metadata = mapOf("item" to candidate.id)
    emitContextMenuFact(Action.CLICK, ContextMenuFacts.Items.ITEM, metadata = metadata)
}

internal fun emitTextSelectionClickFact(optionId: String) {
    val metadata = mapOf("textSelectionOption" to optionId)
    emitContextMenuFact(Action.CLICK, ContextMenuFacts.Items.TEXT_SELECTION_OPTION, metadata = metadata)
}
