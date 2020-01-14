/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.collect
import mozilla.components.concept.push.PushService
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore

/**
 * Migration-aware subscriber that disables the push service during an active migration
 * and re-enables when complete.
 */
class MigrationPushSubscriber(
    private val context: Context,
    private val service: PushService,
    private val store: MigrationStore
) {
    @UseExperimental(ExperimentalCoroutinesApi::class)
    fun start() {
        // Stop the service if it is already started.
        service.stop()

        // Observe for migration completed.
        store.flowScoped { flow ->
            flow.collect { state ->
                if (state.progress == MigrationProgress.COMPLETED) {
                    service.start(context)
                }
            }
        }
    }
}
