/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.flow.collect
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class MigrationTelemetryListener(
    private val metrics: MetricController,
    private val store: MigrationStore
) {
    fun start() {
        // Observe for migration completed.
        store.flowScoped { flow ->
            flow.collect { state ->
                Logger("MigrationTelemetryListener").debug("Migration state: ${state.progress}")
                if (state.progress == MigrationProgress.COMPLETED) {
                    metrics.track(Event.FennecToFenixMigrated)
                }
            }
        }
    }
}
