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
                this.components.core.asyncPasswordsStorage,
                this.components.core.passwordsEncryptionKey
            )
            .migrateFxa(this.components.backgroundServices.accountManager)
            .migrateAddons(this.components.core.engine)
            .migrateTelemetryIdentifiers()
            .build()
    }

    val migrationPushSubscriber by lazy {
        MigrationPushRenewer(
            components.backgroundServices.push,
            components.migrationStore
        )
    }

    override fun setupInMainProcessOnly() {
        // These migrations need to run before regular initialization happens.
        migrateBlocking()

        // Fenix application initialization can happen now.
        super.setupInMainProcessOnly()

        // The rest of the migrations can happen now.
        migrationPushSubscriber.start()
        migrator.startMigrationIfNeeded(components.migrationStore, MigrationService::class.java)
    }

    private fun migrateBlocking() {
        val migrator = FennecMigrator.Builder(this, this.components.analytics.crashReporter)
            .migrateGecko()
            // Telemetry may have been disabled in Fennec, so we need to migrate Settings first
            // to correctly initialize telemetry.
            .migrateSettings()
            .build()

        runBlocking {
            migrator.migrateAsync(components.migrationStore).await()
        }
    }
}

fun Context.getMigratorFromApplication(): FennecMigrator {
    return (applicationContext as MigratingFenixApplication).migrator
}
