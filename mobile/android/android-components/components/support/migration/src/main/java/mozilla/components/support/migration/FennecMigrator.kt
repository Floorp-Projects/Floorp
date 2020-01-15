/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.engine.Engine
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.glean.Glean
import mozilla.components.service.sync.logins.AsyncLoginsStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.FennecMigrator.Builder
import mozilla.components.support.migration.GleanMetrics.MigrationAddons
import mozilla.components.support.migration.state.MigrationAction
import mozilla.components.support.migration.state.MigrationStore
import mozilla.components.support.migration.GleanMetrics.Pings
import mozilla.components.support.migration.GleanMetrics.Migration as MigrationPing
import mozilla.components.support.migration.GleanMetrics.MigrationBookmarks
import mozilla.components.support.migration.GleanMetrics.MigrationFxa
import mozilla.components.support.migration.GleanMetrics.MigrationGecko
import mozilla.components.support.migration.GleanMetrics.MigrationHistory
import mozilla.components.support.migration.GleanMetrics.MigrationLogins
import mozilla.components.support.migration.GleanMetrics.MigrationTelemetryIdentifiers
import mozilla.components.support.migration.GleanMetrics.MigrationOpenTabs
import mozilla.components.support.migration.GleanMetrics.MigrationSettings
import java.io.File
import java.lang.Exception
import java.util.Date
import java.util.UUID
import java.util.concurrent.Executors
import kotlin.IllegalStateException
import kotlin.coroutines.CoroutineContext

/**
 * Supported Fennec migrations and their current versions.
 */
sealed class Migration(val currentVersion: Int) {
    /**
     * Migrates history (both "places" and "visits").
     */
    object History : Migration(currentVersion = 1)

    /**
     * Migrates bookmarks. Must run after history was migrated.
     */
    object Bookmarks : Migration(currentVersion = 1)

    /**
     * Migrates logins.
     */
    object Logins : Migration(currentVersion = 1)

    /**
     * Migrates open tabs.
     */
    object OpenTabs : Migration(currentVersion = 1)

    /**
     * Migrates FxA state.
     */
    object FxA : Migration(currentVersion = 1)

    /**
     * Migrates Gecko(View) internal files.
     */
    object Gecko : Migration(currentVersion = 1)

    /**
     * Migrates all Fennec settings backed by SharedPreferences.
     */
    object Settings : Migration(currentVersion = 1)

    /**
     * Migrates / Disables all currently unsupported Add-ons.
     */
    object Addons : Migration(currentVersion = 1)

    /**
     * Migrates Fennec's telemetry identifiers.
     */
    object TelemetryIdentifiers : Migration(currentVersion = 1)
}

/**
 * Describes a [Migration] at a specific version, enforcing in-range version specification.
 *
 * @property migration A [Migration] in question.
 * @property version Version of the [migration], defaulting to the current version.
 */
data class VersionedMigration(val migration: Migration, val version: Int = migration.currentVersion) {
    init {
        require(version <= migration.currentVersion && version >= 1) {
            "Migration version must be between 1 and current version"
        }
    }
}

/**
 * Exceptions related to Fennec migrations.
 *
 * See https://github.com/mozilla-mobile/android-components/issues/5095 for stripping any possible PII from these
 * exceptions.
 */
sealed class FennecMigratorException(cause: Exception) : Exception(cause) {
    /**
     * Unexpected exception while migrating history.
     * @param cause Original exception which caused the problem.
     */
    class MigrateHistoryException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating bookmarks.
     * @param cause Original exception which caused the problem.
     */
    class MigrateBookmarksException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating logins.
     * @param cause Original exception which caused the problem.
     */
    class MigrateLoginsException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating open tabs.
     * @param cause Original exception which caused the problem.
     */
    class MigrateOpenTabsException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating gecko profile.
     * @param cause Original exception which caused the problem.
     */
    class MigrateGeckoException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating settings.
     * @param cause Original exception which caused the problem.
     */
    class MigrateSettingsException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating addons.
     * @param cause Original exception which caused the problem
     */
    class MigrateAddonsException(cause: Exception) : FennecMigratorException(cause)

    /**
     * Unexpected exception while migrating telemetry identifiers.
     * @param cause Original exception which caused the problem.
     */
    class TelemetryIdentifierException(cause: Exception) : FennecMigratorException(cause)
}

/**
 * Entrypoint for Fennec data migration. See [Builder] for public API.
 *
 * @param context Application context used for accessing the file system.
 * @param migrations Describes ordering and versioning of migrations to run.
 * @param historyStorage An optional instance of [PlacesHistoryStorage] used to store migrated history data.
 * @param bookmarksStorage An optional instance of [PlacesBookmarksStorage] used to store migrated bookmarks data.
 * @param coroutineContext An instance of [CoroutineContext] used for executing async migration tasks.
 */
@Suppress("LargeClass", "TooManyFunctions")
class FennecMigrator private constructor(
    private val context: Context,
    private val crashReporter: CrashReporter,
    private val migrations: List<VersionedMigration>,
    private val historyStorage: PlacesHistoryStorage?,
    private val bookmarksStorage: PlacesBookmarksStorage?,
    private val loginsStorage: AsyncLoginsStorage?,
    private val loginsStorageKey: String?,
    private val sessionManager: SessionManager?,
    private val accountManager: FxaAccountManager?,
    private val engine: Engine?,
    private val profile: FennecProfile?,
    private val fxaState: File?,
    private val browserDbName: String,
    private val signonsDbName: String,
    private val key4DbName: String,
    private val coroutineContext: CoroutineContext
) {
    /**
     * Data migration builder. Allows configuring which migrations to run, their versions and relative order.
     */
    @Suppress("TooManyFunctions")
    class Builder(private val context: Context, private val crashReporter: CrashReporter) {
        private var historyStorage: PlacesHistoryStorage? = null
        private var bookmarksStorage: PlacesBookmarksStorage? = null
        private var loginsStorage: AsyncLoginsStorage? = null
        private var loginsStorageKey: String? = null
        private var sessionManager: SessionManager? = null
        private var accountManager: FxaAccountManager? = null
        private var engine: Engine? = null

        private val migrations: MutableList<VersionedMigration> = mutableListOf()

        // Single-thread executor to ensure we don't accidentally parallelize migrations.
        private var coroutineContext: CoroutineContext = Executors.newSingleThreadExecutor().asCoroutineDispatcher()

        private var fxaState = File("${context.filesDir}", "fxa.account.json")
        private var fennecProfile = FennecProfile.findDefault(context, crashReporter)
        private var browserDbName = "browser.db"
        private var signonsDbName = "signons.sqlite"
        private var key4DbName = "key4.db"
        private var masterPassword = FennecLoginsMigration.DEFAULT_MASTER_PASSWORD

        /**
         * Enable history migration.
         *
         * @param storage An instance of [PlacesHistoryStorage], used for storing data.
         * @param version Version of the migration; defaults to the current version.
         */
        fun migrateHistory(storage: PlacesHistoryStorage, version: Int = Migration.History.currentVersion): Builder {
            check(migrations.find { it.migration is Migration.FxA } == null) {
                "FxA migration, if desired, must run after history"
            }
            historyStorage = storage
            migrations.add(VersionedMigration(Migration.History, version))
            return this
        }

        /**
         * Enable bookmarks migration. Must be called after [migrateHistory].
         *
         * @param storage An instance of [PlacesBookmarksStorage], used for storing data.
         * @param version Version of the migration; defaults to the current version.
         */
        fun migrateBookmarks(
            storage: PlacesBookmarksStorage,
            version: Int = Migration.Bookmarks.currentVersion
        ): Builder {
            check(migrations.find { it.migration is Migration.FxA } == null) {
                "FxA migration, if desired, must run after bookmarks"
            }
            check(migrations.find { it.migration is Migration.History } != null) {
                "To migrate bookmarks, you must first migrate history"
            }
            bookmarksStorage = storage
            migrations.add(VersionedMigration(Migration.Bookmarks, version))
            return this
        }

        /**
         * Enable logins migration.
         *
         * @param storage An instance of [AsyncLoginsStorage], used for storing data.
         */
        fun migrateLogins(
            storage: AsyncLoginsStorage,
            storageKey: String,
            version: Int = Migration.Logins.currentVersion
        ): Builder {
            check(migrations.find { it.migration is Migration.FxA } == null) {
                "FxA migration, if desired, must run after logins"
            }
            loginsStorage = storage
            loginsStorageKey = storageKey
            migrations.add(VersionedMigration(Migration.Logins, version))
            return this
        }

        /**
         * Enables the migration of Gecko internal files.
         */
        fun migrateGecko(
            version: Int = Migration.Gecko.currentVersion
        ): Builder {
            migrations.add(VersionedMigration(Migration.Gecko, version))
            return this
        }

        /**
         * Enable open tabs migration.
         *
         * @param sessionManager An instance of [SessionManager] used for restoring migrated [SessionManager.Snapshot].
         * @param version Version of the migration; defaults to the current version.
         */
        fun migrateOpenTabs(sessionManager: SessionManager, version: Int = Migration.OpenTabs.currentVersion): Builder {
            this.sessionManager = sessionManager
            migrations.add(VersionedMigration(Migration.OpenTabs, version))
            return this
        }

        /**
         * Enable FxA state migration.
         *
         * @param accountManager An instance of [FxaAccountManager] used for authenticating using a migrated account.
         * @param version Version of the migration; defaults to the current version.
         */
        fun migrateFxa(accountManager: FxaAccountManager, version: Int = Migration.FxA.currentVersion): Builder {
            this.accountManager = accountManager
            migrations.add(VersionedMigration(Migration.FxA, version))
            return this
        }

        /**
         * Enable all Fennec - Fenix common settings migration.
         */
        fun migrateSettings(version: Int = Migration.Settings.currentVersion): Builder {
            migrations.add(VersionedMigration(Migration.Settings, version))
            return this
        }

        /**
         * Enable migration of Fennec telemetry identifiers.
         */
        fun migrateTelemetryIdentifiers(
            version: Int = Migration.TelemetryIdentifiers.currentVersion
        ): Builder {
            migrations.add(VersionedMigration(Migration.TelemetryIdentifiers, version))
            return this
        }

        /**
         * Enables Add-on migration.
         *
         * @param engine an instance of [Engine] use to query installed add-ons.
         * @param version Version of the migration; defaults to the current version.
         */
        fun migrateAddons(engine: Engine, version: Int = Migration.Settings.currentVersion): Builder {
            this.engine = engine
            migrations.add(VersionedMigration(Migration.Addons, version))
            return this
        }

        /**
         * Constructs a [FennecMigrator] based on the current configuration.
         */
        fun build(): FennecMigrator {
            return FennecMigrator(
                context,
                crashReporter,
                migrations,
                historyStorage,
                bookmarksStorage,
                loginsStorage,
                loginsStorageKey,
                sessionManager,
                accountManager,
                engine,
                fennecProfile,
                fxaState,
                browserDbName,
                signonsDbName,
                key4DbName,
                coroutineContext
            )
        }

        // The rest of the setters are useful for unit tests.
        @VisibleForTesting
        internal fun setCoroutineContext(coroutineContext: CoroutineContext): Builder {
            this.coroutineContext = coroutineContext
            return this
        }

        @VisibleForTesting
        internal fun setBrowserDbName(name: String): Builder {
            browserDbName = name
            return this
        }

        @VisibleForTesting
        internal fun setSignonsDbName(name: String): Builder {
            signonsDbName = name
            return this
        }

        @VisibleForTesting
        internal fun setKey4DbName(name: String): Builder {
            key4DbName = name
            return this
        }

        @VisibleForTesting
        internal fun setProfile(profile: FennecProfile): Builder {
            fennecProfile = profile
            return this
        }

        @VisibleForTesting
        internal fun setFxaState(state: File): Builder {
            fxaState = state
            return this
        }
    }

    private val logger = Logger("FennecMigrator")

    // Used to ensure migration runs do not overlap.
    private val migrationLock = Object()

    private val dbPath = profile?.let { "${it.path}/$browserDbName" }

    /**
     * Performs configured data migration. See [Builder] for how to configure a data migration.
     *
     * @return A deferred [MigrationResults], describing which migrations were performed and if they succeeded.
     */
    fun migrateAsync(): Deferred<MigrationResults> = synchronized(migrationLock) {
        val migrationsToRun = getMigrationsToRun()

        // Short-circuit if there's nothing to do.
        if (migrationsToRun.isEmpty()) {
            logger.debug("No migrationz to run.")
            val result = CompletableDeferred<MigrationResults>()
            result.complete(emptyMap())
            return result
        }

        return runMigrationsAsync(migrationsToRun)
    }

    /**
     * Returns true if there are migrations to run for this installation.
     */
    private fun hasMigrationsToRun(): Boolean {
        return getMigrationsToRun().isNotEmpty()
    }

    private fun isFennecInstallation(): Boolean {
        // Not implemented yet.
        // https://github.com/mozilla-mobile/android-components/issues/5115
        return true
    }

    /**
     * If a migration is needed then invoking this method will update the [MigrationStore] and launch
     * the provided [AbstractMigrationService] implementation.
     */
    fun <T> startMigrationIfNeeded(
        store: MigrationStore,
        service: Class<T>
    ) where T : AbstractMigrationService {
        if (!isFennecInstallation()) {
            // This installation seems to never have been Fennec, so we do not need
            // to migrate anything.
            logger.debug("This is not a Fennec installation. No migration needed.")
            return
        }

        if (!hasMigrationsToRun()) {
            // There are no migrations to run for this installation. This likely means that we are
            // migrated already and there are no updated migrations to run.
            logger.debug("This is a Fennec installation. But there are no migrations to run.")
            return
        }

        logger.debug("Migration is needed. Updating state and starting service.")

        runBlocking {
            store.dispatch(MigrationAction.Started).join()
        }

        ContextCompat.startForegroundService(context, Intent(context, service))
    }

    private fun getMigrationsToRun(): List<VersionedMigration> {
        val migrationRecord = MigrationResultsStore(context)
        val migrationHistory = migrationRecord.getCached()

        // Either didn't run before, or ran with an older version than current migration's version.
        return migrations.filter { versionedMigration ->
            val pastVersion = migrationHistory?.get(versionedMigration.migration)?.version
            if (pastVersion == null) {
                true
            } else {
                versionedMigration.version > pastVersion
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun runMigrationsAsync(
        migrations: List<VersionedMigration>
    ): Deferred<MigrationResults> = CoroutineScope(coroutineContext).async {

        // Note that we're depending on coroutineContext to be backed by a single-threaded executor, in order to ensure
        // non-overlapping execution of our migrations.

        val migrationStore = MigrationResultsStore(context)
        val results = mutableMapOf<Migration, MigrationRun>()

        migrations.forEach { versionedMigration ->
            logger.debug("Executing $versionedMigration")

            val telemetryId = versionedMigration.migration.telemetryIdentifier()
            val migrationVersion = versionedMigration.version
            MigrationPing.migrationVersions[telemetryId].set("$migrationVersion")

            val migrationResult = when (versionedMigration.migration) {
                Migration.History -> migrateHistory()
                Migration.Bookmarks -> migrateBookmarks()
                Migration.OpenTabs -> migrateOpenTabs()
                Migration.FxA -> migrateFxA()
                Migration.Gecko -> migrateGecko()
                Migration.Logins -> migrateLogins()
                Migration.Settings -> migrateSharedPrefs()
                Migration.Addons -> migrateAddons()
                Migration.TelemetryIdentifiers -> migrateTelemetryIdentifiers()
            }

            val migrationRun = when (migrationResult) {
                is Result.Failure<*> -> {
                    logger.error(
                        "Failed to migrate $versionedMigration",
                        migrationResult.throwables.first()
                    )
                    // Local fun to make `when` exhaustive.
                    fun setFailure(migration: Migration): Unit = when (migration) {
                        Migration.History -> MigrationHistory.anyFailures.set(true)
                        Migration.Bookmarks -> MigrationBookmarks.anyFailures.set(true)
                        Migration.OpenTabs -> MigrationOpenTabs.anyFailures.set(true)
                        Migration.FxA -> MigrationFxa.anyFailures.set(true)
                        Migration.Gecko -> MigrationGecko.anyFailures.set(true)
                        Migration.Logins -> MigrationLogins.anyFailures.set(true)
                        Migration.Settings -> MigrationSettings.anyFailures.set(true)
                        Migration.Addons -> MigrationAddons.anyFailures.set(true)
                        Migration.TelemetryIdentifiers -> MigrationTelemetryIdentifiers.anyFailures.set(true)
                    }
                    setFailure(versionedMigration.migration)

                    MigrationRun(versionedMigration.version, false)
                }
                is Result.Success<*> -> {
                    logger.debug(
                        "Migrated $versionedMigration"
                    )
                    MigrationRun(versionedMigration.version, true)
                }
            }

            // Save result of this migration immediately, so that we keep it even if we crash later
            // in the process and do not rerun this migration version.
            migrationStore.setOrUpdate(mapOf(versionedMigration.migration to migrationRun))

            results[versionedMigration.migration] = migrationRun
        }

        // At this point, individual migrations have populated the MigrationResultPing, and we can
        // ask Glean to send it.
        Pings.migration.submit()

        results
    }

    @SuppressWarnings("TooGenericExceptionCaught", "MagicNumber")
    private fun migrateHistory(): Result<Unit> {
        checkNotNull(historyStorage) { "History storage must be configured to migrate history" }

        // There's no dbPath without a profile, but if a profile is present we expect dbPath to be also present.
        if (profile != null && dbPath == null) {
            crashReporter.submitCaughtException(IllegalStateException("Missing DB path during history migration"))
        }

        if (dbPath == null) {
            return Result.Failure(IllegalStateException("Missing DB path during history migration"))
        }
        val migrationMetrics = try {
            logger.debug("Migrating history...")
            historyStorage.importFromFennec(dbPath)
        } catch (e: Exception) {
            crashReporter.submitCaughtException(FennecMigratorException.MigrateHistoryException(e))
            return Result.Failure(e)
        }

        // Process migration metrics. Here and elsewhere, we're assuming and hard-coding metrics schema.
        // See application-services repository: https://github.com/mozilla/application-services/commit/a7d5ff1903fb0f904785a1645cb7ae1d6c313f10
        try {
            MigrationHistory.detected.add(migrationMetrics.getInt("num_total"))
            MigrationHistory.migrated["succeeded"].add(migrationMetrics.getInt("num_succeeded"))
            MigrationHistory.migrated["failed"].add(migrationMetrics.getInt("num_failed"))
            // Assuming that 'total_duration' is in milliseconds.
            MigrationHistory.duration.setRawNanos(migrationMetrics.getLong("total_duration") * 1000000)
        } catch (e: Exception) {
            MigrationHistory.anyFailures.set(true)
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateHistoryException(e)
            )
        }

        logger.debug("Migrated history.")
        return Result.Success(Unit)
    }

    @SuppressWarnings("TooGenericExceptionCaught", "MagicNumber")
    private fun migrateBookmarks(): Result<Unit> {
        checkNotNull(bookmarksStorage) { "Bookmarks storage must be configured to migrate bookmarks" }

        // There's no dbPath without a profile, but if a profile is present we expect dbPath to be also present.
        if (profile != null && dbPath == null) {
            crashReporter.submitCaughtException(IllegalStateException("Missing DB path during bookmark migration"))
        }

        if (dbPath == null) {
            return Result.Failure(IllegalStateException("Missing DB path during bookmark migration"))
        }
        val migrationMetrics = try {
            logger.debug("Migrating bookmarks...")
            bookmarksStorage.importFromFennec(dbPath)
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateBookmarksException(e)
            )
            return Result.Failure(e)
        }

        // Process migration metrics. Here and elsewhere, we're assuming and hard-coding metrics schema.
        // See application-services repository: https://github.com/mozilla/application-services/commit/b2e2edcc06a04503d493e1733b0d566815feac7c#diff-216f62325632ae6549587b038b21cfe0
        try {
            MigrationBookmarks.detected.add(migrationMetrics.getInt("num_total"))
            MigrationBookmarks.migrated["succeeded"].add(migrationMetrics.getInt("num_succeeded"))
            MigrationBookmarks.migrated["failed"].add(migrationMetrics.getInt("num_failed"))
            // Assuming that 'total_duration' is in milliseconds.
            MigrationBookmarks.duration.setRawNanos(migrationMetrics.getLong("total_duration") * 1000000)
        } catch (e: Exception) {
            MigrationBookmarks.anyFailures.set(true)
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateBookmarksException(e)
            )
        }

        logger.debug("Migrated history.")
        return Result.Success(Unit)
    }

    @Suppress("ComplexMethod", "TooGenericExceptionCaught", "LongMethod", "ReturnCount")
    private suspend fun migrateLogins(): Result<LoginsMigrationResult> {
        if (profile == null) {
            crashReporter.submitCaughtException(IllegalStateException("Missing Profile path"))
            return Result.Failure(IllegalStateException("Missing Profile path"))
        }

        val result = try {
            logger.debug("Migrating logins...")
            FennecLoginsMigration.migrate(
                crashReporter,
                signonsDbPath = "${profile.path}/$signonsDbName",
                key4DbPath = "${profile.path}/$key4DbName",
                loginsStorage = loginsStorage!!,
                loginsStorageKey = loginsStorageKey!!
            )
        } catch (e: Exception) {
            crashReporter.submitCaughtException(FennecMigratorException.MigrateLoginsException(e))
            return Result.Failure(e)
        }

        if (result is Result.Failure<LoginsMigrationResult>) {
            val migrationFailureWrapper = result.throwables.first() as LoginMigrationException
            return when (val failure = migrationFailureWrapper.failure) {
                is LoginsMigrationResult.Failure.FailedToCheckMasterPassword -> {
                    logger.error("Failed to check master password: $failure")
                    MigrationLogins.failureReason.add(FailureReasonTelemetryCodes.LOGINS_MP_CHECK.code)
                    // We definitely expect to be able to check our master password, so report a failure.
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
                is LoginsMigrationResult.Failure.UnsupportedSignonsDbVersion -> {
                    logger.error("Unsupported logins database version: $failure")
                    MigrationLogins.failureReason.add(FailureReasonTelemetryCodes.LOGINS_UNSUPPORTED_LOGINS_DB.code)
                    MigrationLogins.unsupportedDbVersion.add(failure.version)
                    // We really don't expect anyone to hit this, so let's submit it to Sentry.
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
                is LoginsMigrationResult.Failure.UnexpectedLoginsKeyMaterialAlg,
                is LoginsMigrationResult.Failure.UnexpectedMetadataKeyMaterialAlg -> {
                    logger.error("Encryption failure: $failure")
                    MigrationLogins.failureReason.add(FailureReasonTelemetryCodes.LOGINS_ENCRYPTION.code)
                    // While this may happen in theory, let's keep track of exact reasons.
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
                is LoginsMigrationResult.Failure.GetLoginsThrew -> {
                    logger.error("getLogins failure: $failure")
                    MigrationLogins.failureReason.add(FailureReasonTelemetryCodes.LOGINS_GET.code)
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
                is LoginsMigrationResult.Failure.RustImportThrew -> {
                    logger.error("Rust import failure: $failure")
                    MigrationLogins.failureReason.add(FailureReasonTelemetryCodes.LOGINS_RUST_IMPORT.code)
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
            }
        }

        val loginMigrationSuccess = result as Result.Success<LoginsMigrationResult>
        return when (val success = loginMigrationSuccess.value as LoginsMigrationResult.Success) {
            is LoginsMigrationResult.Success.MasterPasswordIsSet -> {
                logger.debug("Could not migrate logins - master password is set")
                MigrationLogins.successReason.add(SuccessReasonTelemetryCodes.LOGINS_MP_SET.code)
                result
            }

            is LoginsMigrationResult.Success.ImportedLoginRecords -> {
                logger.debug("""Imported login records! Details:
                    Total detected=${success.totalRecordsDetected},
                    failed to process=${success.failedToProcess},
                    failed to import=${success.failedToImport}
                """.trimIndent())
                MigrationLogins.successReason.add(SuccessReasonTelemetryCodes.LOGINS_MIGRATED.code)
                MigrationLogins.detected.add(success.totalRecordsDetected)
                MigrationLogins.failureCounts["process"].add(success.failedToProcess)
                MigrationLogins.failureCounts["import"].add(success.failedToImport)
                result
            }
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    private suspend fun migrateOpenTabs(): Result<SessionManager.Snapshot> {
        if (profile == null) {
            crashReporter.submitCaughtException(IllegalStateException("Missing Profile path"))
            return Result.Failure(IllegalStateException("Missing Profile path"))
        }

        return try {
            logger.debug("Migrating session...")
            val result = FennecSessionMigration.migrate(File(profile.path))
            if (result is Result.Success<SessionManager.Snapshot>) {
                logger.debug("Loading migrated session snapshot...")
                MigrationOpenTabs.detected.add(result.value.sessions.size)
                withContext(Dispatchers.Main) {
                    sessionManager!!.restore(result.value)
                    // Note that this is assuming that sessionManager starts off empty before the
                    // migration.
                    MigrationOpenTabs.migrated.add(sessionManager.all.size)
                }
            } else if (result is Result.Failure<*>) {
                MigrationOpenTabs.anyFailures.set(result.throwables.isNotEmpty())
            }
            result
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateOpenTabsException(e)
            )
            Result.Failure(e)
        }
    }

    private suspend fun migrateFxA(): Result<FxaMigrationResult> {
        val result = FennecFxaMigration.migrate(fxaState!!, context, accountManager!!)

        if (result is Result.Failure<FxaMigrationResult>) {
            val migrationFailureWrapper = result.throwables.first() as FxaMigrationException
            return when (val failure = migrationFailureWrapper.failure) {
                is FxaMigrationResult.Failure.CorruptAccountState -> {
                    logger.error("Detected a corrupt account state: $failure")
                    MigrationFxa.failureReason.add(FailureReasonTelemetryCodes.FXA_CORRUPT_ACCOUNT_STATE.code)
                    result
                }
                is FxaMigrationResult.Failure.UnsupportedVersions -> {
                    logger.error("Detected unsupported versions: $failure")
                    MigrationFxa.failureReason.add(FailureReasonTelemetryCodes.FXA_UNSUPPORTED_VERSIONS.code)
                    MigrationFxa.unsupportedAccountVersion.set("${failure.accountVersion}")
                    MigrationFxa.unsupportedPickleVersion.set("${failure.pickleVersion}")
                    MigrationFxa.unsupportedStateVersion.set("${failure.stateVersion}")
                    result
                }
                is FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount -> {
                    logger.error("Failed to sign-in into an authenticated account")
                    MigrationFxa.failureReason.add(FailureReasonTelemetryCodes.FXA_SIGN_IN_FAILED.code)
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    result
                }
                is FxaMigrationResult.Failure.CustomServerConfigPresent -> {
                    logger.error("Custom config present: token=${failure.customTokenServer}," +
                        "idp=${failure.customIdpServer}")
                    MigrationFxa.failureReason.add(FailureReasonTelemetryCodes.FXA_CUSTOM_SERVER.code)
                    MigrationFxa.hasCustomIdpServer.set(failure.customIdpServer)
                    MigrationFxa.hasCustomTokenServer.set(failure.customTokenServer)
                    result
                }
            }
        }

        val migrationSuccess = result as Result.Success<FxaMigrationResult>
        return when (val success = migrationSuccess.value as FxaMigrationResult.Success) {
            // The rest are all successful migrations.
            is FxaMigrationResult.Success.NoAccount -> {
                logger.debug("No Fennec account detected")
                MigrationFxa.successReason.add(SuccessReasonTelemetryCodes.FXA_NO_ACCOUNT.code)
                result
            }
            is FxaMigrationResult.Success.UnauthenticatedAccount -> {
                // Here we have an 'email' and a state label.
                // "Bad auth state" could be a few things - unverified account, bad credentials detected by Fennec, etc.
                // We could try using the 'email' address as a starting point in the authentication flow.
                logger.debug("Detected a Fennec account in a bad authentication state: ${success.stateLabel}")
                MigrationFxa.successReason.add(SuccessReasonTelemetryCodes.FXA_BAD_AUTH.code)
                MigrationFxa.badAuthState.set(success.stateLabel)
                result
            }
            is FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount -> {
                logger.debug("Signed-in into a detected Fennec account")
                MigrationFxa.successReason.add(SuccessReasonTelemetryCodes.FXA_SIGNED_IN.code)
                result
            }
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    private fun migrateGecko(): Result<Unit> {
        if (profile == null) {
            return Result.Failure(IllegalStateException("Missing Profile path"))
        }

        return try {
            logger.debug("Migrating gecko files...")
            val result = GeckoMigration.migrate(profile.path)
            logger.debug("Migrated gecko files.")
            result
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateGeckoException(e)
            )
            Result.Failure(e)
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    private fun migrateSharedPrefs(): Result<SettingsMigrationResult> {
        val result = try {
            FennecSettingsMigration.migrateSharedPrefs(context)
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateSettingsException(e)
            )
            return Result.Failure(e)
        }

        if (
            result is Result.Success<SettingsMigrationResult> &&
            result.value is SettingsMigrationResult.Success.SettingsMigrated
        ) {
            logger.info("Preferences migrated, telemetry should be: " + result.value.telemetry)

            // We let Glean immediately know about the updated telemetry value. We expect it to
            // have been initialized with telemetry OFF earlier and so we may enable it here
            // because further migration code will try to send telemetry pings too - and this
            // will happen before we hand off to the app again to continue and enable telemetry.
            Glean.setUploadEnabled(result.value.telemetry)
        }

        if (result is Result.Failure<SettingsMigrationResult>) {
            val migrationFailureWrapper = result.throwables.first() as SettingsMigrationException
            return when (val failure = migrationFailureWrapper.failure) {
                is SettingsMigrationResult.Failure.MissingFHRPrefValue -> {
                    logger.error("Missing FHR value: $failure")
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    MigrationSettings.failureReason.add(FailureReasonTelemetryCodes.SETTINGS_MISSING_FHR_VALUE.code)
                    result
                }
                is SettingsMigrationResult.Failure.WrongTelemetryValueAfterMigration -> {
                    logger.error("Wrong telemetry value: $failure")
                    crashReporter.submitCaughtException(migrationFailureWrapper)
                    MigrationSettings.failureReason.add(FailureReasonTelemetryCodes.SETTINGS_WRONG_TELEMETRY_VALUE.code)
                    result
                }
            }
        }

        val migrationSuccess = result as Result.Success<SettingsMigrationResult>
        return when (val success = migrationSuccess.value as SettingsMigrationResult.Success) {
            // The rest are all successful migrations.
            is SettingsMigrationResult.Success.NoFennecPrefs -> {
                logger.debug("No Fennec prefs detected")
                MigrationSettings.successReason.add(SuccessReasonTelemetryCodes.SETTINGS_NO_PREFS.code)
                result
            }
            is SettingsMigrationResult.Success.SettingsMigrated -> {
                logger.debug("Migrated settings; telemetry=${success.telemetry}")
                MigrationSettings.successReason.add(SuccessReasonTelemetryCodes.SETTINGS_MIGRATED.code)
                MigrationSettings.telemetryEnabled.set(success.telemetry)
                result
            }
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught", "NestedBlockDepth", "ComplexMethod")
    private suspend fun migrateAddons(): Result<AddonMigrationResult> {
        return try {
            logger.debug("Migrating add-ons...")
            val result = AddonMigration.migrate(engine!!)
            if (result is Result.Failure<AddonMigrationResult>) {
                val migrationFailureWrapper = result.throwables.first() as AddonMigrationException
                return when (val failure = migrationFailureWrapper.failure) {
                    is AddonMigrationResult.Failure.FailedToQueryInstalledAddons -> {
                        logger.error("Failed to query installed add-ons: $failure")
                        MigrationAddons.failureReason.add(FailureReasonTelemetryCodes.ADDON_QUERY.code)
                        crashReporter.submitCaughtException(migrationFailureWrapper)
                        result
                    }
                    is AddonMigrationResult.Failure.FailedToMigrateAddons -> {
                        logger.error("Failed to migrate some add-ons: $failure")
                        MigrationAddons.failedAddons.add(failure.failedAddons.size)
                        MigrationAddons.migratedAddons.add(failure.migratedAddons.size)
                        val recordedFailures = mutableSetOf<String>()
                        failure.failedAddons.forEach { (_, exception) ->
                            // Let's not spam Sentry and submit the same exception multiple times
                            if (recordedFailures.add(exception.uniqueId())) {
                                crashReporter.submitCaughtException(
                                    FennecMigratorException.MigrateAddonsException(exception)
                                )
                            }
                        }
                        result
                    }
                }
            }

            val migrationSuccess = result as Result.Success<AddonMigrationResult>
            return when (val success = migrationSuccess.value as AddonMigrationResult.Success) {
                is AddonMigrationResult.Success.NoAddons -> {
                    logger.debug("No add-ons to migrate")
                    MigrationAddons.successReason.add(SuccessReasonTelemetryCodes.ADDONS_NO.code)
                    result
                }
                is AddonMigrationResult.Success.AddonsMigrated -> {
                    MigrationAddons.successReason.add(SuccessReasonTelemetryCodes.ADDONS_MIGRATED.code)
                    MigrationAddons.migratedAddons.add(success.migratedAddons.size)
                    logger.debug("Successfully migrated add-ons")
                    result
                }
            }
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.MigrateAddonsException(e)
            )
            Result.Failure(e)
        }
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    private fun migrateTelemetryIdentifiers(): Result<TelemetryIdentifiersResult> {
        if (profile == null) {
            crashReporter.submitCaughtException(IllegalStateException("Missing Profile path"))
            return Result.Failure(IllegalStateException("Missing Profile path"))
        }

        val result = try {
            // Will submit unexpected errors via crashReporter.
            TelemetryIdentifiersMigration.migrate(profile.path, crashReporter)
        } catch (e: Exception) {
            crashReporter.submitCaughtException(
                FennecMigratorException.TelemetryIdentifierException(e)
            )
            return Result.Failure(e)
        }

        if (result is Result.Success<TelemetryIdentifiersResult>) {
            when (val success = result.value as TelemetryIdentifiersResult.Success) {
                is TelemetryIdentifiersResult.Success.Identifiers -> {
                    // It's important that we're aware, during telemetry analysis, that these values
                    // are missing or present. Absence of a set value should be enough.
                    success.clientId?.let { MigrationTelemetryIdentifiers.fennecClientId.set(UUID.fromString(it)) }
                    // profileCreationDate is a unix timestamp.
                    success.profileCreationDate?.let {
                        MigrationTelemetryIdentifiers.fennecProfileCreationDate.set(Date(it))
                    }
                }
            }
        }

        return result
    }
}
