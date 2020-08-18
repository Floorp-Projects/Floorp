/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import kotlinx.coroutines.runBlocking
import mozilla.components.support.migration.FennecMigrator
import org.mozilla.fenix.session.PerformanceActivityLifecycleCallbacks
import org.mozilla.fenix.migration.MigrationTelemetryListener

/**
 * An application class which knows how to migrate Fennec data.
 */
class MigratingFenixApplication : FenixApplication() {
    init {
        recordOnInit() // DO NOT MOVE ANYTHING ABOVE HERE: the timing of this measurement is critical.

        PerformanceActivityLifecycleCallbacks.isTransientActivityInMigrationVariant = {
            if (it is MigrationDecisionActivity) true else false
        }
    }

    val migrator by lazy {
        FennecMigrator.Builder(this, this.components.analytics.crashReporter)
            .migrateOpenTabs(this.components.core.sessionManager)
            .migrateHistory(this.components.core.lazyHistoryStorage)
            .migrateBookmarks(
                this.components.core.lazyBookmarksStorage,
                this.components.core.pinnedSiteStorage
            )
            .migrateLogins(this.components.core.lazyPasswordsStorage)
            .migrateFxa(lazy { this.components.backgroundServices.accountManager })
            .migrateAddons(
                this.components.core.engine,
                this.components.addonCollectionProvider,
                this.components.addonUpdater
            )
            .migrateTelemetryIdentifiers()
            .migrateSearchEngine(this.components.search.searchEngineManager)
            .build()
    }

    val migrationPushSubscriber by lazy {
        MigrationPushRenewer(
            components.push.feature,
            components.migrationStore
        )
    }

    val migrationTelemetryListener by lazy {
        MigrationTelemetryListener(
            components.analytics.metrics,
            components.migrationStore
        )
    }

    override fun setupInMainProcessOnly() {
        // These migrations need to run before regular initialization happens.
        migrateBlocking()

        // Now that we have migrated from Fennec whether the user wants to enable telemetry we can
        // initialize Glean
        initializeGlean()

        // Fenix application initialization can happen now.
        super.setupInMainProcessOnly()

        // The rest of the migrations can happen now.
        migrationPushSubscriber.start()
        migrationTelemetryListener.start()
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
