/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import kotlinx.coroutines.flow.collect
import mozilla.components.concept.push.PushProcessor
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.state.MigrationProgress
import mozilla.components.support.migration.state.MigrationStore

/**
 * Force-renews push subscription after migration was complete.
 */
class MigrationPushRenewer(
    private val service: PushProcessor?,
    private val store: MigrationStore
) {
    fun start() {
        // Observe for migration completed.
        store.flowScoped { flow ->
            flow.collect { state ->
                Logger("MigrationPushRenewer").debug("Migration state: ${state.progress}")
                if (state.progress == MigrationProgress.COMPLETED) {
                    Logger("MigrationPushRenewer").debug("Renewing registration....")

                    // This should force a recreation of firebase device token, re-registration with
                    // the autopush service, and subsequent update of the FxA device record with
                    // new push subscription information.
                    service?.renewRegistration()
                }
            }
        }
    }
}
