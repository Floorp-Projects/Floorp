/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class GeckoMigrationTest {
    @Test
    fun `No existing files is treated as successful migration`() {
        val result = GeckoMigration.migrate(getTestPath("empty").absolutePath)
        assertTrue(result is Result.Success)
    }

    @Test
    fun `Prefs file will be deleted after migration`() {
        val profile = File(getTestPath("combined"), "basic")
        val prefsFile = File(profile, "prefs.js")

        assertTrue(prefsFile.exists())

        val result = GeckoMigration.migrate(profile.absolutePath)
        assertTrue(result is Result.Success)

        assertFalse(prefsFile.exists())
    }
}
