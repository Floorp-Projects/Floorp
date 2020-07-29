/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.support.ktx.util.writeString
import java.io.File
import java.io.IOException

private const val PREFS_FILE = "prefs.js"
private const val PREFS_BACKUP_FILE = "prefs.js.backup.v"

@VisibleForTesting
internal var userPrefsToKeep = setOf("extensions.webextensions.uuids")
private const val USER_PREF = "user_pref"

/**
 * Wraps [GeckoMigrationResult] in an exception so that it can be returned via [Result.Failure].
 *
 * @property failure Wrapped [GeckoMigrationResult] indicating the exact failure reason.
 */
class GeckoMigrationException(val failure: GeckoMigrationResult.Failure) : Exception(failure.toString())

/**
 * Result of a Gecko migration.
 */
sealed class GeckoMigrationResult {
    /**
     * Success variants of a Gecko migration.
     */
    sealed class Success : GeckoMigrationResult() {
        /**
         * No prefs.js file present.
         */
        object NoPrefsFile : Success()

        /**
         * prefs.js file was removed as it contained no prefs we wanted to keep.
         */
        object PrefsFileRemovedNoPrefs : Success()

        /**
         * prefs.js file was removed as we failed to transform prefs.
         */
        object PrefsFileRemovedInvalidPrefs : Success()

        /**
         * prefs.js file was successfully migrated with prefs we wanted to keep.
         */
        object PrefsFileMigrated : Success()
    }

    /**
     * Failure variants of a Gecko migration.
     */
    sealed class Failure : GeckoMigrationResult() {
        /**
         * Failed to write backup file for prefs.js.
         */
        internal data class FailedToWriteBackup(val throwable: Throwable) : Failure() {
            override fun toString(): String {
                return "Failed to write backup of prefs.js file: $throwable"
            }
        }

        /**
         * Failed to write transformed prefs.js file.
         */
        internal data class FailedToWritePrefs(val throwable: Throwable) : Failure() {
            override fun toString(): String {
                return "Failed to write transformed prefs.js file: $throwable"
            }
        }

        /**
         * Failed to delete prefs.js file.
         */
        internal data class FailedToDeleteFile(val throwable: Throwable? = null) : Failure() {
            override fun toString(): String {
                return "Failed to delete prefs.js file: ${throwable ?: ""}"
            }
        }
    }
}

/**
 * Helper for migrating Gecko related internal files.
 */
internal object GeckoMigration {
    /**
     * Tries to migrate Gecko related internal files in [profilePath].
     */
    fun migrate(
        profilePath: String,
        migrationVersion: Int
    ): Result<GeckoMigrationResult> {
        // GeckoView will happily pick up the profile from Fennec and reuse all data in it. So this
        // migration is mostly focused on removing all prefs that we do not want to reuse.
        val prefsjs = File(profilePath, PREFS_FILE)
        if (!prefsjs.exists()) {
            return Result.Success(GeckoMigrationResult.Success.NoPrefsFile)
        }

        // First, let's create a backup of the file.
        try {
            prefsjs.copyTo(File(profilePath, PREFS_BACKUP_FILE + migrationVersion), true)
        } catch (e: IOException) {
            return Result.Failure(GeckoMigrationException(GeckoMigrationResult.Failure.FailedToWriteBackup(e)))
        }

        // Now let's transform the original to contain only prefs we want to keep.
        val transformed = prefsjs.useLines {
            it.filter { line ->
                userPrefsToKeep.any { pref -> line.startsWith("$USER_PREF(\"$pref\"") }
            }.toList()
        }

        return if (transformed.isEmpty()) {
            removePrefsFile(prefsjs)
        } else {
            // Run a very basic validation and fall back to removing the file if we find anything wrong.
            val invalid = transformed.any { line -> !isLineValid(line) }
            if (invalid) {
                removePrefsFile(prefsjs, invalid = true)
            } else {
                rewritePrefsFile(prefsjs, transformed)
            }
        }
    }

    private fun isLineValid(pref: String): Boolean {
        // Make sure that
        // - user_pref exists exactly once at the beginning of the line
        // - lines ends with ');'
        return pref.lastIndexOf("$USER_PREF") == 0 && pref.endsWith(");")
    }

    @Suppress("TooGenericExceptionCaught")
    private fun removePrefsFile(prefsJs: File, invalid: Boolean = false): Result<GeckoMigrationResult> {
        return try {
            if (prefsJs.delete()) {
                return if (invalid) {
                    Result.Success(GeckoMigrationResult.Success.PrefsFileRemovedInvalidPrefs)
                } else {
                    Result.Success(GeckoMigrationResult.Success.PrefsFileRemovedNoPrefs)
                }
            } else {
                Result.Failure(GeckoMigrationException(GeckoMigrationResult.Failure.FailedToDeleteFile()))
            }
        } catch (e: SecurityException) {
            Result.Failure(GeckoMigrationException(GeckoMigrationResult.Failure.FailedToDeleteFile(e)))
        }
    }

    private fun rewritePrefsFile(prefsJs: File, prefs: List<String>): Result<GeckoMigrationResult> {
        val atomicPrefsJs = AtomicFile(prefsJs)
        val newPrefs = prefs.joinToString(separator = "\n")
        return if (atomicPrefsJs.writeString { newPrefs }) {
            Result.Success(GeckoMigrationResult.Success.PrefsFileMigrated)
        } else {
            Result.Failure(GeckoMigrationException(
                GeckoMigrationResult.Failure.FailedToWritePrefs(IOException("$PREFS_FILE could not be rewritten"))
            ))
        }
    }
}
