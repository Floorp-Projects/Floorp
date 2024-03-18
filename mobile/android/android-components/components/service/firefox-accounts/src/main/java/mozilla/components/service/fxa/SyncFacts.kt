/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to FxA Sync operations.
 */
class SyncFacts {

    /**
     * Specific types of telemetry items.
     */
    object Items {
        const val SYNC_FAILED = "sync_failed"
    }
}

private fun emitSyncFact(
    action: Action,
    item: String,
    value: String? = null,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.SERVICE_FIREFOX_ACCOUNTS,
        action,
        item,
        value,
        metadata,
    ).collect()
}

internal fun emitSyncFailedFact() = emitSyncFact(Action.INTERACTION, SyncFacts.Items.SYNC_FAILED)
