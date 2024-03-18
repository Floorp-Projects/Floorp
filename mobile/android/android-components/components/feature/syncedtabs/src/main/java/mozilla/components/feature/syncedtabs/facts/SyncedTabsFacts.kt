/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.facts

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to the Synced Tabs feature.
 */
class SyncedTabsFacts {
    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val SYNCED_TABS_SUGGESTION_CLICKED = "synced_tabs_suggestion_clicked"
    }
}

private fun emitSyncedTabsFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.FEATURE_SYNCEDTABS,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitSyncedTabSuggestionClickedFact() {
    emitSyncedTabsFact(
        Action.INTERACTION,
        SyncedTabsFacts.Items.SYNCED_TABS_SUGGESTION_CLICKED,
    )
}
