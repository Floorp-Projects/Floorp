/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.migration.FennecSettingsMigration.FENIX_PREFS_EXPERIMENTATION_KEY
import mozilla.components.support.migration.FennecSettingsMigration.FENIX_PREFS_MARKETING_TELEMETRY_KEY
import mozilla.components.support.migration.FennecSettingsMigration.FENIX_PREFS_TELEMETRY_KEY
import mozilla.components.support.migration.FennecSettingsMigration.FENIX_SHARED_PREFS_NAME
import mozilla.components.support.migration.FennecSettingsMigration.FENNEC_APP_SHARED_PREFS_NAME
import mozilla.components.support.migration.FennecSettingsMigration.FENNEC_PREFS_FHR_KEY
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class FennecSettingsMigrationTest {
    @After
    fun `Clean up all SharedPreferences`() {
        File(testContext.filesDir.parent, "shared_prefs").delete()
    }

    @Test
    fun `Missing Fennec preferences entirely is treated as success`() {
        with(FennecSettingsMigration.migrateSharedPrefs(testContext) as Result.Success) {
            assertEquals(SettingsMigrationResult.Success.NoFennecPrefs::class, this.value::class)
        }
    }

    @Test
    fun `Missing FHR pref value is treated as failure`() {
        val fennecAppPrefs = testContext.getSharedPreferences(FENNEC_APP_SHARED_PREFS_NAME, Context.MODE_PRIVATE)

        // Make prefs non-empty.
        fennecAppPrefs.edit().putString("dummy", "key").apply()

        with(FennecSettingsMigration.migrateSharedPrefs(testContext) as Result.Failure) {
            val unwrapped = this.throwables.first() as SettingsMigrationException
            assertEquals(SettingsMigrationResult.Failure.MissingFHRPrefValue::class, unwrapped.failure::class)
        }
    }

    @Test
    fun `Fennec FHR disabled will set Fenix telemetry as stopped and return SUCCESS`() {
        assertFHRMigration(isFHREnabled = false)
    }

    @Test
    fun `Fennec FHR enabled will set Fenix telemetry as active and return SUCCESS`() {
        assertFHRMigration(isFHREnabled = true)
    }

    private fun assertFHRMigration(isFHREnabled: Boolean) {
        val fennecAppPrefs = testContext.getSharedPreferences(FENNEC_APP_SHARED_PREFS_NAME, Context.MODE_PRIVATE)
        val fenixPrefs = testContext.getSharedPreferences(FENIX_SHARED_PREFS_NAME, Context.MODE_PRIVATE)

        fennecAppPrefs.edit().putBoolean(FENNEC_PREFS_FHR_KEY, isFHREnabled).commit()

        with(FennecSettingsMigration.migrateSharedPrefs(testContext) as Result.Success) {
            assertEquals(SettingsMigrationResult.Success.SettingsMigrated::class, this.value::class)
            val v = this.value as SettingsMigrationResult.Success.SettingsMigrated
            assertEquals(isFHREnabled, v.telemetry)
        }

        assertEquals(isFHREnabled, fenixPrefs.getBoolean(FENIX_PREFS_TELEMETRY_KEY, !isFHREnabled))
        assertEquals(isFHREnabled, fenixPrefs.getBoolean(FENIX_PREFS_MARKETING_TELEMETRY_KEY, !isFHREnabled))
        assertEquals(isFHREnabled, fenixPrefs.getBoolean(FENIX_PREFS_EXPERIMENTATION_KEY, !isFHREnabled))
    }
}
