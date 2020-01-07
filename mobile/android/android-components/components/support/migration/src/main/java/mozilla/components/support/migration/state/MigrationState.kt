/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration.state

import mozilla.components.lib.state.State
import mozilla.components.support.migration.MigrationResults

/**
 * Value type that represents the state of the migration.
 */
data class MigrationState(
    val progress: MigrationProgress = MigrationProgress.NONE,
    val results: MigrationResults? = null
) : State

/**
 * The progress of the migration.
 */
enum class MigrationProgress {
    /**
     * No migration is happening.
     */
    NONE,

    /**
     * Migration is in progress.
     */
    MIGRATING,

    /**
     * Migration has completed.
     */
    COMPLETED
}
