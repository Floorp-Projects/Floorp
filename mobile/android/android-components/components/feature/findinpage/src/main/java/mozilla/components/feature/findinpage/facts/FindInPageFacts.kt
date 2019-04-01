/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

private fun emitFindInPageFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null
) {
    Fact(
        Component.FEATURE_FINDINPAGE,
        action,
        item,
        value,
        metadata
    ).collect()
}

private object FindInPageItems {
    const val PREVIOUS = "previous"
    const val NEXT = "next"
    const val CLOSE = "close"
    const val INPUT = "input"
}

internal fun emitCloseFact() = emitFindInPageFact(Action.CLICK, FindInPageItems.CLOSE)
internal fun emitNextFact() = emitFindInPageFact(Action.CLICK, FindInPageItems.NEXT)
internal fun emitPreviousFact() = emitFindInPageFact(Action.CLICK, FindInPageItems.PREVIOUS)
internal fun emitCommitFact(value: String) =
        emitFindInPageFact(Action.COMMIT, FindInPageItems.INPUT, value)
