/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger

/**
 * Helper for migrating all common Fennec - Fenix settings.
 */
internal object FennecSettingsMigration {
    private val logger = Logger("FennecSettingsMigration")
    private lateinit var fenixAppPrefs: SharedPreferences
    private lateinit var fennecAppPrefs: SharedPreferences

    @VisibleForTesting const val FENNEC_APP_SHARED_PREFS_NAME = "GeckoApp"
    @VisibleForTesting const val FENIX_SHARED_PREFS_NAME = "fenix_preferences"
    const val FENNEC_PREFS_FHR_KEY = "android.not_a_preference.healthreport.uploadEnabled"
    const val FENIX_PREFS_TELEMETRY_KEY = "pref_key_telemetry"

    /**
     * Migrate all Fennec - Fenix common SharedPreferences.
     */
    fun migrateSharedPrefs(context: Context): Result<SettingsMigrationResult> {
        fennecAppPrefs = context.getSharedPreferences(FENNEC_APP_SHARED_PREFS_NAME, MODE_PRIVATE)
        fenixAppPrefs = context.getSharedPreferences(FENIX_SHARED_PREFS_NAME, MODE_PRIVATE)

        if (fennecAppPrefs.all.isEmpty()) {
            logger.info("No Fennec prefs, bailing out.")
            return Result.Success(SettingsMigrationResult.Success.NoFennecPrefs)
        }

        return migrateAppPreferences()
    }

    private fun migrateAppPreferences(): Result<SettingsMigrationResult> {
        return migrateTelemetryOptInStatus(fennecAppPrefs, fenixAppPrefs)
    }

    private fun migrateTelemetryOptInStatus(
        fennecPrefs: SharedPreferences,
        fenixPrefs: SharedPreferences
    ): Result<SettingsMigrationResult> {
        // Sanity check: make sure we actually have an FHR value set.
        if (!fennecPrefs.contains(FENNEC_PREFS_FHR_KEY)) {
            logger.warn("Missing FHR pref value")
            return Result.Failure(SettingsMigrationException(SettingsMigrationResult.Failure.MissingFHRPrefValue))
        }

        // Fennec has two telemetry settings:
        // - Firefox Health Report (FHR) - defaults to 'on',
        // - Telemetry - defaults to 'off'.
        // These two settings control different parts of the telemetry systems in Fennec. The reality is that even
        // though default for "telemetry" is off, since FHR defaults to "on", by default Fennec collects a non-trivial
        // subset of available in-product telemetry.
        // Since the Telemetry pref defaults to 'off', it's impossible to distinguish between "user disabled telemetry"
        // and "user didn't touch the setting".
        // So we use FHR value as a proxy for telemetry overall.
        // If FHR is disabled by the user, we'll disable telemetry in Fenix. Otherwise, it will be enabled.

        // Read Fennec prefs.
        val fennecFHRState = fennecPrefs.getBoolean(FENNEC_PREFS_FHR_KEY, false)
        logger.info("Fennec FHR state is: $fennecFHRState")

        // Update Fenix prefs.
        fenixPrefs.edit()
            .putBoolean(FENIX_PREFS_TELEMETRY_KEY, fennecFHRState)
            .apply()

        // Make sure it worked.
        if (fenixPrefs.getBoolean(FENIX_PREFS_TELEMETRY_KEY, !fennecFHRState) != fennecFHRState) {
            logger.warn("Wrong telemetry value after migration")
            return Result.Failure(
                SettingsMigrationException(
                    SettingsMigrationResult.Failure.WrongTelemetryValueAfterMigration(expected = fennecFHRState)
                )
            )
        }

        // Done!
        return Result.Success(SettingsMigrationResult.Success.SettingsMigrated(telemetry = fennecFHRState))
    }
}

/**
 * Wraps [SettingsMigrationResult] in an exception so that it can be returned via [Result.Failure].
 *
 * @property failure Wrapped [SettingsMigrationResult] indicating exact failure reason.
 */
class SettingsMigrationException(val failure: SettingsMigrationResult.Failure) : Exception(failure.toString())

/**
 * Result of Fennec settings migration.
 */
sealed class SettingsMigrationResult {
    /**
     * Successful setting migration.
     */
    sealed class Success : SettingsMigrationResult() {
        /**
         * Fennec app SharedPreference file is not accessible.
         *
         * This means this is a fresh install of Fenix, not an update from Fennec.
         * Nothing to migrate.
         */
        object NoFennecPrefs : Success() {
            override fun toString(): String {
                return "No previous app settings. Nothing to migrate."
            }
        }

        /**
         * Migration work completed successfully.
         */
        data class SettingsMigrated(val telemetry: Boolean) : Success() {
            override fun toString(): String {
                return "Previous Fennec settings migrated successfully; value: $telemetry"
            }
        }
    }

    /**
     * Failed settings migrations.
     */
    sealed class Failure : SettingsMigrationResult() {
        /**
         * Couldn't find FHR pref value in non-empty Fennec prefs.
         */
        object MissingFHRPrefValue : Failure() {
            override fun toString(): String {
                return "Missing FHR pref value"
            }
        }

        /**
         * Wrong telemetry value in Fenix after migration.
         */
        data class WrongTelemetryValueAfterMigration(val expected: Boolean) : Failure() {
            override fun toString(): String {
                return "Wrong telemetry pref value after migration. Expected $expected."
            }
        }
    }
}
