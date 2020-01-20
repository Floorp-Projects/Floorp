/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration.state

import mozilla.components.lib.state.Action
import mozilla.components.support.migration.Migration
import mozilla.components.support.migration.MigrationRun

/**
 * Actions supported by the [MigrationStore].
 */
sealed class MigrationAction : Action {
    /**
     * A migration is needed and has been started.
     */
    object Started : MigrationAction()

    /**
     * A migration was completed.
     */
    object Completed : MigrationAction()

    /**
     * Clear (or reset) the migration state.
     */
    object Clear : MigrationAction()

    /**
     * Set the result of a migration run.
     */
    data class MigrationRunResult(
        val migration: Migration,
        val run: MigrationRun
    ) : MigrationAction()
}
