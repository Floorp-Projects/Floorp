/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Intent
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore

/**
 * An [IntentProcessor] that checks if we're in a migration state.
 *
 * ⚠️ When using this processor, ensure this is the first processor to be invoked if there are multiple.
 */
class MigrationIntentProcessor(private val store: MigrationStore) : IntentProcessor {

    /**
     * Matches itself with all intents in order to ensure processing all of incoming intents.
     */
    override fun matches(intent: Intent): Boolean = store.state.progress == MigrationProgress.MIGRATING

    /**
     * Processes all incoming intents if a migration is in progress.
     *
     * If this is true, we should show an instance of AbstractMigrationProgressActivity.
     */
    override suspend fun process(intent: Intent): Boolean {
        return when (store.state.progress) {
            MigrationProgress.COMPLETED, MigrationProgress.NONE -> false
            MigrationProgress.MIGRATING -> true
        }
    }
}
