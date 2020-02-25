/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import mozilla.components.support.migration.AbstractMigrationService
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.ext.components

/**
 * Background service for running the migration from legacy Firefox for Android (Fennec).
 */
class MigrationService : AbstractMigrationService() {
    override val migrator by lazy { getMigratorFromApplication() }
    override val store: MigrationStore by lazy { components.migrationStore }
    override val migrationActivity = MigrationDecisionActivity::class.java
}
