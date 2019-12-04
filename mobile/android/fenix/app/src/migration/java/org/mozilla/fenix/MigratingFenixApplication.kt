/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import kotlinx.coroutines.runBlocking
import mozilla.components.support.migration.FennecMigrator

/**
 * An application class which knows how to migrate Fennec data.
 */
class MigratingFenixApplication : FenixApplication() {
    val migrator by lazy {
        FennecMigrator.Builder(this, this.components.analytics.crashReporter)
            .migrateOpenTabs(this.components.core.sessionManager)
            .migrateHistory(this.components.core.historyStorage)
            .migrateBookmarks(this.components.core.bookmarksStorage)
            .migrateLogins(
                this.components.core.passwordsStorage.store,
                this.components.core.passwordsEncryptionKey
            )
            .migrateFxa(this.components.backgroundServices.accountManager)
            .build()
    }

    override fun setupInMainProcessOnly() {
        migrateGeckoBlocking()

        super.setupInMainProcessOnly()

        migrator.startMigrationServiceIfNeeded(MigrationService::class.java)
    }

    private fun migrateGeckoBlocking() {
        val migrator = FennecMigrator.Builder(this, this.components.analytics.crashReporter)
            .migrateGecko()
            .build()

        runBlocking {
            migrator.migrateAsync().await()
        }
    }
}

fun Context.getMigratorFromApplication(): FennecMigrator {
    return (applicationContext as MigratingFenixApplication).migrator
}
