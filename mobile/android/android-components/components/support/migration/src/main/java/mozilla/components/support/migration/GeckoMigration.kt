/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import java.io.File
import java.io.IOException

private const val PREFS_FILE = "prefs.js"

/**
 * Helper for migrating Gecko related internal files.
 */
internal object GeckoMigration {
    /**
     * Tries to migrate Gecko related internal files in [profilePath].
     */
    fun migrate(
        profilePath: String
    ): Result<Unit> {
        // GeckoView will happily pick up the profile from Fennec and reuse all data in it. So this
        // migration is mostly focused on removing the files that we do not want to reuse.

        // The prefs file contains gecko prefs that have been changed by the user (about:config) or
        // code. Since those may be incompatible with GeckoView and/or app code, we decided to just
        // reset them. GeckoView will recreate the file with defaults if needed.
        return removePrefsFile(File(profilePath))
    }

    private fun removePrefsFile(profilePath: File): Result<Unit> {
        val prefsFile = File(profilePath, PREFS_FILE)
        if (prefsFile.exists()) {
            return if (prefsFile.delete()) {
                Result.Success(Unit)
            } else {
                Result.Failure(IOException("$PREFS_FILE exists but could not be deleted"))
            }
        }

        return Result.Success(Unit)
    }
}
