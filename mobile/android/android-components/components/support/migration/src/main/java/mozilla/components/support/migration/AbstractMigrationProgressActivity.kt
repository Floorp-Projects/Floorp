/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore

/**
 * An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor].
 *
 * Implementation notes: this abstraction exists to ensure that migration apps do not allow the user to
 * invoke [AppCompatActivity.onBackPressed].
 */
abstract class AbstractMigrationProgressActivity : AppCompatActivity(), MigrationStateListener {

    protected abstract val store: MigrationStore

    private val observer: MigrationObserver by lazy {
        MigrationObserver(store, this)
    }

    override fun onStart() {
        super.onStart()
        observer.start()
    }

    override fun onStop() {
        observer.stop()
        super.onStop()
    }

    // Override and do nothing so that you can't exit out of the migration UI.
    final override fun onBackPressed() = Unit
}

/**
 * Interface to be implemented by classes that want to observe the migration state changes.
 */
interface MigrationStateListener {

    /**
     * Invoked on a migration state change.
     *
     * @param progress The progress of the entire migration process.
     * @param results The updated results from any progress made.
     */
    fun onMigrationStateChanged(progress: MigrationProgress, results: MigrationResults)

    /**
     * Invoked when the migration is complete.
     */
    fun onMigrationCompleted(results: MigrationResults)
}

internal class MigrationObserver(
    private val store: MigrationStore,
    private val listener: MigrationStateListener
) {
    private var scope: CoroutineScope? = null

    fun start() {
        scope = store.flowScoped { flow ->
            flow.collect { state ->
                val results = state.results.orEmpty()

                if (state.progress == MigrationProgress.MIGRATING) {
                    listener.onMigrationStateChanged(state.progress, results)
                } else {
                    listener.onMigrationCompleted(results)
                }
            }
        }
    }

    fun stop() = scope?.cancel()
}
