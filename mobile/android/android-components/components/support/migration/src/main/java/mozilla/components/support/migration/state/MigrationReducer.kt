/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration.state

internal object MigrationReducer {
    internal fun reduce(state: MigrationState, action: MigrationAction): MigrationState {
        return when (action) {
            is MigrationAction.Started -> state.copy(
                progress = MigrationProgress.MIGRATING
            )
            is MigrationAction.Completed -> state.copy(
                progress = MigrationProgress.COMPLETED
            )
            is MigrationAction.Clear -> state.copy(
                progress = MigrationProgress.NONE
            )
            is MigrationAction.Result -> state.copy(
                results = action.results
            )
        }
    }
}
