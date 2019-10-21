/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import java.lang.IllegalArgumentException

class VersionedMigrationTest {
    @Test
    fun `version defaults to current`() {
        assertEquals(Migration.History.currentVersion, VersionedMigration(Migration.History).version)
        assertEquals(Migration.Bookmarks.currentVersion, VersionedMigration(Migration.Bookmarks).version)
        assertEquals(Migration.OpenTabs.currentVersion, VersionedMigration(Migration.OpenTabs).version)
    }

    @Test
    fun `version range is enforced`() {
        for (migration in listOf(Migration.History, Migration.Bookmarks, Migration.OpenTabs)) {
            assertEquals(1, VersionedMigration(migration, 1).version)

            try {
                VersionedMigration(migration, 10)
                fail()
            } catch (e: IllegalArgumentException) {}

            try {
                VersionedMigration(migration, 0)
                fail()
            } catch (e: IllegalArgumentException) {}
        }
    }
}