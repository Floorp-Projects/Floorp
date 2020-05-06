/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class GeckoMigrationTest {
    @Test
    fun `No existing files is treated as successful migration`() {
        val result = GeckoMigration.migrate(getTestPath("empty").absolutePath, 1)
        assertTrue(result is Result.Success)
        assertTrue((result as Result.Success).value is GeckoMigrationResult.Success.NoPrefsFile)
    }

    @Test
    fun `Prefs file will be deleted if no prefs should be kept`() {
        userPrefsToKeep = setOf("extensions.webextensions.uuids")

        val profile = File(getTestPath("prefs").absolutePath, "noaddons")
        val prefsFile = File(profile, "prefs.js")
        assertTrue(prefsFile.exists())

        val result = GeckoMigration.migrate(profile.absolutePath, 1)
        assertTrue(result is Result.Success)
        assertTrue((result as Result.Success).value is GeckoMigrationResult.Success.PrefsFileRemovedNoPrefs)
        assertFalse(prefsFile.exists())
    }

    @Test
    fun `Prefs file will be deleted if validation of transformed file fails`() {
        userPrefsToKeep = setOf("test.keep.valid", "test.keep.invalid")

        var profile = File(getTestPath("prefs").absolutePath, "invalid")
        var prefsFile = File(profile, "prefs.js")
        assertTrue(prefsFile.exists())

        var result = GeckoMigration.migrate(profile.absolutePath, 1)
        assertTrue(result is Result.Success)
        assertFalse(prefsFile.exists())

        profile = File(getTestPath("prefs"), "invalid-nonewlines")
        prefsFile = File(profile, "prefs.js")
        assertTrue(prefsFile.exists())

        result = GeckoMigration.migrate(profile.absolutePath, 1)
        assertTrue(result is Result.Success)
        assertTrue((result as Result.Success).value is GeckoMigrationResult.Success.PrefsFileRemovedInvalidPrefs)
        assertFalse(prefsFile.exists())
    }

    @Test
    fun `Original prefs file is kept as backup`() {
        val profile = File(getTestPath("prefs"), "backup")
        val prefsFile = File(profile, "prefs.js")
        assertTrue(prefsFile.exists())

        val result = GeckoMigration.migrate(profile.absolutePath, 1)
        assertTrue(result is Result.Success)

        val prefsBackupFile = File(profile, "prefs.js.backup.v1")
        assertTrue(prefsBackupFile.exists())
    }

    @Test
    fun `Prefs file only contains prefs we want to keep`() {
        userPrefsToKeep = setOf("test.keep", "extensions.webextensions.uuids")

        val profile = File(getTestPath("prefs").absolutePath, "migrate")
        val result = GeckoMigration.migrate(profile.absolutePath, 1)
        assertTrue(result is Result.Success)
        assertTrue((result as Result.Success).value is GeckoMigrationResult.Success.PrefsFileMigrated)

        val prefsFile = File(profile, "prefs.js")
        assertTrue(prefsFile.exists())

        val transformedPrefs = prefsFile.readText()

        val testPrefToKeep = "user_pref(\"test.keep\", \"This is a test value\");"
        val extensionPrefToKeep = "user_pref(\"extensions.webextensions.uuids\", \"{\\\"webcompat-reporter@mozilla.org\\\":\\\"45cc1bb1-6607-4e41-bdcf-1306f5573bec\\\",\\\"webcompat@mozilla.org\\\":\\\"ec5d19c6-cefd-4155-a6fd-d5af7bb5ab2f\\\",\\\"default-theme@mozilla.org\\\":\\\"cb12a42f-a4da-49af-a7ae-09418136ab49\\\"}\");"
        assertEquals(2, transformedPrefs.lines().size)
        assertEquals(extensionPrefToKeep, transformedPrefs.lines()[0])
        assertEquals(testPrefToKeep, transformedPrefs.lines()[1])
    }
}
