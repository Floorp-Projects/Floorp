/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the AwesomeBar feature.
 */
class AwesomeBarFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val BOOKMARK_SUGGESTION_CLICKED = "bookmark_suggestion_clicked"
        const val CLIPBOARD_SUGGESTION_CLICKED = "clipboard_suggestion_clicked"
        const val HISTORY_SUGGESTION_CLICKED = "history_suggestion_clicked"
        const val SEARCH_ACTION_CLICKED = "search_action_clicked"
        const val SEARCH_SUGGESTION_CLICKED = "search_suggestion_clicked"
        const val OPENED_TAB_SUGGESTION_CLICKED = "opened_tab_suggestion_clicked"
    }
}

private fun emitAwesomebarFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_AWESOMEBAR,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitBookmarkSuggestionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.BOOKMARK_SUGGESTION_CLICKED,
    )
}

internal fun emitClipboardSuggestionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.CLIPBOARD_SUGGESTION_CLICKED,
    )
}

internal fun emitHistorySuggestionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED,
    )
}

internal fun emitSearchActionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.SEARCH_ACTION_CLICKED,
    )
}

internal fun emitSearchSuggestionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED,
    )
}

internal fun emitOpenTabSuggestionClickedFact() {
    emitAwesomebarFact(
        Action.INTERACTION,
        AwesomeBarFacts.Items.OPENED_TAB_SUGGESTION_CLICKED,
    )
}
