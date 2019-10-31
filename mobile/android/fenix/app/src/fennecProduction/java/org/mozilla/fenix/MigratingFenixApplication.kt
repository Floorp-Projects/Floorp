/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import mozilla.components.support.ktx.android.content.isMainProcess
import mozilla.components.support.migration.FennecMigrator

/**
 * An application class which knows how to migrate Fennec data.
 */
class MigratingFenixApplication : FenixApplication() {
    override fun setupApplication() {
        super.setupApplication()

        // Same check as is present in super.setupApplication:
        if (!isMainProcess()) {
            // If this is not the main process then do not continue with the migration here.
            // Migration only needs to be done in our app's main process and should not be done in other processes like
            // a GeckoView child process or the crash handling process. Most importantly we never want to end up in a
            // situation where we create a GeckoRuntime from the Gecko child process.
            return
        }

        val migrator = FennecMigrator.Builder(this, this.components.analytics.crashReporter)
            .migrateOpenTabs(this.components.core.sessionManager)
            .migrateHistory(this.components.core.historyStorage)
            .migrateBookmarks(this.components.core.bookmarksStorage)
            .migrateFxa(this.components.backgroundServices.accountManager)
            .build()

        migrator.migrateAsync()
    }
}
