/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

private fun emitToolbarFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null
) {
    Fact(
        Component.BROWSER_TOOLBAR,
        action,
        item,
        value,
        metadata
    ).collect()
}

private object ToolbarItems {
    const val TOOLBAR = "toolbar"
    const val MENU = "menu"
}

internal fun emitOpenMenuFact() = emitToolbarFact(Action.CLICK, ToolbarItems.MENU)

internal fun emitCommitFact(
    autocompleteResult: InlineAutocompleteEditText.AutocompleteResult?
) {
    val metadata = if (autocompleteResult == null) {
        mapOf(
            "autocomplete" to false
        )
    } else {
        mapOf(
            "autocomplete" to true,
            "source" to autocompleteResult.source
        )
    }

    emitToolbarFact(Action.COMMIT, ToolbarItems.TOOLBAR, metadata = metadata)
}
