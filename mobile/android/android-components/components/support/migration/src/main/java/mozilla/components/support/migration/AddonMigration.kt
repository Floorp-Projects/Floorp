/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.update.AddonUpdater

@VisibleForTesting
internal var supportedAddonsFallback = listOf("uBlock0@raymondhill.net")

@VisibleForTesting
internal const val AMO_READ_TIMEOUT_IN_SECONDS = 5L

/**
 * Wraps [AddonMigrationResult] in an exception so that it can be returned via [Result.Failure].
 *
 * @property failure Wrapped [AddonMigrationResult] indicating exact failure reason.
 */
class AddonMigrationException(val failure: AddonMigrationResult.Failure) : Exception(failure.toString())

/**
 * Result of an add-on migration.
 */
sealed class AddonMigrationResult {
    /**
     * Success variants of an add-on migration.
     */
    sealed class Success : AddonMigrationResult() {
        /**
         * No add-ons installed to migrate.
         */
        object NoAddons : Success()

        /**
         * Successfully migrated add-ons.
         */
        data class AddonsMigrated(
            val migratedAddons: List<WebExtension>
        ) : Success() {
            override fun toString(): String {
                return "Successfully migrated ${migratedAddons.size} add-ons."
            }
        }
    }

    /**
     * Failure variants of an add-on migration.
     */
    sealed class Failure : AddonMigrationResult() {
        /**
         * Failed to query installed add-ons via the [Engine].
         */
        internal data class FailedToQueryInstalledAddons(val throwable: Throwable) : Failure() {
            override fun toString(): String {
                return "Failed to query installed add-ons: ${throwable::class}"
            }
        }

        /**
         * Failed to migrate some or all of the installed add-ons.
         */
        internal data class FailedToMigrateAddons(
            val migratedAddons: List<WebExtension>,
            val failedAddons: Map<WebExtension, Exception>
        ) : Failure() {
            override fun toString(): String {
                return "Failed to migrate ${failedAddons.size} of ${failedAddons.size + migratedAddons.size} add-ons."
            }
        }
    }
}

/**
 * Helper for migrating add-ons.
 */
internal object AddonMigration {

    /**
     * Performs a migration of all installed add-ons. Migration involves:
     * - Disabling unsupported add-ons
     * - Enabling supported add-ons that where unsupported in previous migration runs
     * - Registering enabled add-ons for automatic updates
     */
    @SuppressWarnings("TooGenericExceptionCaught", "ComplexMethod")
    suspend fun migrate(
        engine: Engine,
        addonCollectionProvider: AddonCollectionProvider,
        addonUpdater: AddonUpdater
    ): Result<AddonMigrationResult> {
        val installedAddons =
        try {
            getInstalledAddons(engine)
        } catch (e: Exception) {
            return Result.Failure(AddonMigrationException(AddonMigrationResult.Failure.FailedToQueryInstalledAddons(e)))
        }

        if (installedAddons.isEmpty()) {
            return Result.Success(AddonMigrationResult.Success.NoAddons)
        }

        val supportedAddonIds = try {
            addonCollectionProvider.getAvailableAddons(
                readTimeoutInSeconds = AMO_READ_TIMEOUT_IN_SECONDS
            ).mapNotNull { it.id }
        } catch (e: Exception) {
            supportedAddonsFallback
        }

        val migratedAddons = mutableListOf<WebExtension>()
        val failures = mutableMapOf<WebExtension, Exception>()
        installedAddons.filter { !it.isBuiltIn() }.forEach { addon ->
            try {
                val migratedAddon = migrateAddon(engine, addon, supportedAddonIds)
                if (migratedAddon.isEnabled()) {
                    addonUpdater.registerForFutureUpdates(migratedAddon.id)
                }
                migratedAddons += migratedAddon
            } catch (e: Exception) {
                failures[addon] = e
            }
        }

        return if (failures.isEmpty()) {
            Result.Success(AddonMigrationResult.Success.AddonsMigrated(migratedAddons))
        } else {
            val failure = AddonMigrationResult.Failure.FailedToMigrateAddons(migratedAddons, failures)
            Result.Failure(AddonMigrationException(failure))
        }
    }

    private suspend fun getInstalledAddons(engine: Engine): List<WebExtension> {
        return CompletableDeferred<List<WebExtension>>().also { result ->
            withContext(Dispatchers.Main) {
                engine.listInstalledWebExtensions(
                    onSuccess = { result.complete(it) },
                    onError = { result.completeExceptionally(it) }
                )
            }
        }.await()
    }

    private suspend fun migrateAddon(engine: Engine, addon: WebExtension, supportedAddons: List<String>): WebExtension {
        if (supportedAddons.contains(addon.id)) {
            // Enable the add-on if it was unsupported and disabled in a previous migration run
            if (addon.getMetadata()?.disabledFlags?.contains(DisabledFlags.APP_SUPPORT) == true) {
                return CompletableDeferred<WebExtension>().also { result ->
                    withContext(Dispatchers.Main) {
                        engine.enableWebExtension(
                            addon,
                            EnableSource.APP_SUPPORT,
                            onSuccess = { result.complete(it) },
                            onError = { result.completeExceptionally(it) }
                        )
                    }
                }.await()
            }
            // No need to do anything if the add-on is supported and not disabled in a previous migration run
            return addon
        } else {
            // Disable the add-on if it is unsupported
            return CompletableDeferred<WebExtension>().also { result ->
                withContext(Dispatchers.Main) {
                    engine.disableWebExtension(
                        addon,
                        EnableSource.APP_SUPPORT,
                        onSuccess = { result.complete(it) },
                        onError = { result.completeExceptionally(it) }
                    )
                }
            }.await()
        }
    }
}
