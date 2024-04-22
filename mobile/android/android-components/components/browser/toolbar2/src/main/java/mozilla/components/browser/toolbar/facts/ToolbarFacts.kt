/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

/**
 * Facts emitted for telemetry related to [ToolbarFeature]
 */
class ToolbarFacts {
    /**
     * Items that specify which portion of the [ToolbarFeature] was interacted with
     */
    object Items {
        const val TOOLBAR = "toolbar"
        const val MENU = "menu"
    }
}

private fun emitToolbarFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.BROWSER_TOOLBAR,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitOpenMenuFact(extras: Map<String, Any>?) {
    emitToolbarFact(Action.CLICK, ToolbarFacts.Items.MENU, metadata = extras)
}

internal fun emitCommitFact(
    autocompleteResult: InlineAutocompleteEditText.AutocompleteResult?,
) {
    val metadata = if (autocompleteResult == null) {
        mapOf(
            "autocomplete" to false,
        )
    } else {
        mapOf(
            "autocomplete" to true,
            "source" to autocompleteResult.source,
        )
    }

    emitToolbarFact(Action.COMMIT, ToolbarFacts.Items.TOOLBAR, metadata = metadata)
}
