/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect

/**
 * Facts emitted for telemetry related to migration.
 */
class MigrationFacts {

    /**
     * Items that specify which portion of the migration was invoked.
     */
    object Items {
        const val MIGRATION_COMPLETED = "migration_completed"
        const val MIGRATION_STARTED = "migration_started"
    }
}

private fun emitMigrationFact(
    action: Action,
    item: String
) {
    Fact(
        Component.SUPPORT_MIGRATION,
        action,
        item
    ).collect()
}

internal fun emitCompletedFact() = emitMigrationFact(Action.INTERACTION, MigrationFacts.Items.MIGRATION_COMPLETED)
internal fun emitStartedFact() = emitMigrationFact(Action.INTERACTION, MigrationFacts.Items.MIGRATION_STARTED)
