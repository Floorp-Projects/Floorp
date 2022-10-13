/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to [FindInPageFeature]
 */
class FindInPageFacts {
    /**
     * Items that specify which portion of the [FindInPageFeature] was interacted with
     */
    object Items {
        const val PREVIOUS = "previous"
        const val NEXT = "next"
        const val CLOSE = "close"
        const val INPUT = "input"
    }
}

private fun emitFindInPageFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_FINDINPAGE,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitCloseFact() = emitFindInPageFact(Action.CLICK, FindInPageFacts.Items.CLOSE)
internal fun emitNextFact() = emitFindInPageFact(Action.CLICK, FindInPageFacts.Items.NEXT)
internal fun emitPreviousFact() = emitFindInPageFact(Action.CLICK, FindInPageFacts.Items.PREVIOUS)
internal fun emitCommitFact(value: String) =
    emitFindInPageFact(Action.COMMIT, FindInPageFacts.Items.INPUT, value)
