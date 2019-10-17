/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.PlacesException
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.File
import java.lang.IllegalStateException

@RunWith(AndroidJUnit4::class)
class FennecMigratorTest {
    @Test
    fun `no-op migration`() = runBlocking {
        val migrator = FennecMigrator.Builder(testContext)
            .setCoroutineContext(this.coroutineContext)
            .build()

        // Can do this once.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }

        // Can do this all day long!
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `history migration must be done before bookmarks`() = runBlocking {
        try {
            FennecMigrator.Builder(testContext)
                .setCoroutineContext(this.coroutineContext)
                .migrateBookmarks(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}

        try {
            FennecMigrator.Builder(testContext)
                .setCoroutineContext(this.coroutineContext)
                .migrateBookmarks(mock())
                .migrateHistory(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}
    }

    @Test
    fun `migrations versioning basics`() = runBlocking {
        val historyStore = PlacesHistoryStorage(testContext)
        val bookmarksStore = PlacesBookmarksStorage(testContext)

        val migrator = FennecMigrator.Builder(testContext)
            .setCoroutineContext(this.coroutineContext)
            .setProfile(FennecProfile(
                "test", File(getTestPath("combined"), "basic").absolutePath, true)
            )
            .setBrowserDbName("browser.db")
            .migrateHistory(historyStore)
            .migrateBookmarks(bookmarksStore)
            .build()

        assertTrue(historyStore.getVisited().isEmpty())
        assertTrue(bookmarksStore.searchBookmarks("mozilla").isEmpty())

        // Can run once.
        with(migrator.migrateAsync().await()) {
            assertEquals(2, this.size)

            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))

            with(this.getValue(Migration.History)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            with(this.getValue(Migration.Bookmarks)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }
        }

        assertEquals(6, historyStore.getVisited().size)
        assertEquals(4, bookmarksStore.searchBookmarks("mozilla").size)

        // Do not run again for the same version.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }

        assertEquals(6, historyStore.getVisited().size)
        assertEquals(4, bookmarksStore.searchBookmarks("mozilla").size)

        // Can add another migration type, and it will be the only one to run.
        val sessionManager: SessionManager = mock()
        val expandedMigrator = FennecMigrator.Builder(testContext)
            .setCoroutineContext(this.coroutineContext)
            .setProfile(FennecProfile(
                "test",
                File(getTestPath("combined"),
                    "basic"
                ).absolutePath,
                true
            ))
            .migrateHistory(historyStore)
            .migrateBookmarks(bookmarksStore)
            .migrateOpenTabs(sessionManager)
            .build()

        with(expandedMigrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.OpenTabs))

            with(this.getValue(Migration.OpenTabs)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            verify(sessionManager, times(1)).restore(mozilla.components.support.test.any(), anyBoolean())
        }

        // Running this migrator again does nothing.
        with(expandedMigrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 1`() = runBlocking {
        val historyStorage: PlacesHistoryStorage = mock()

        // DB path/profile are not configured correctly for the test environment
        val migrator = FennecMigrator.Builder(testContext)
            .migrateHistory(historyStorage)
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertFalse(this.getValue(Migration.History).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 2`() = runBlocking {
        val historyStorage: PlacesHistoryStorage = mock()

        // Fail during history migration.
        `when`(historyStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))

        val migrator = FennecMigrator.Builder(testContext)
            .migrateHistory(historyStorage)
            .setCoroutineContext(this.coroutineContext)
            .setProfile(FennecProfile(
                "test",
                File(getTestPath("combined"),
                    "basic"
                ).absolutePath,
                true
            ))
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertFalse(this.getValue(Migration.History).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 3`() = runBlocking {
        val bookmarkStorage: PlacesBookmarksStorage = mock()
        val historyStorage: PlacesHistoryStorage = mock()

        // DB path missing.
        val migrator = FennecMigrator.Builder(testContext)
            .migrateHistory(historyStorage)
            .migrateBookmarks(bookmarkStorage)
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(2, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertFalse(this.getValue(Migration.History).success)
            assertFalse(this.getValue(Migration.Bookmarks).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 4`() = runBlocking {
        val bookmarkStorage: PlacesBookmarksStorage = mock()
        val historyStorage: PlacesHistoryStorage = mock()

        // Fail during history migration.
        `when`(historyStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))

        // DB path is configured, partial success (only history failed).
        val migrator = FennecMigrator.Builder(testContext)
            .migrateHistory(historyStorage)
            .migrateBookmarks(bookmarkStorage)
            .setCoroutineContext(this.coroutineContext)
            .setProfile(FennecProfile(
                "test",
                File(getTestPath("combined"),
                    "basic"
                ).absolutePath,
                true
            ))
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(2, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertFalse(this.getValue(Migration.History).success)
            assertTrue(this.getValue(Migration.Bookmarks).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 5`() = runBlocking {
        val bookmarkStorage: PlacesBookmarksStorage = mock()
        val historyStorage: PlacesHistoryStorage = mock()

        // Both migrations failed.
        `when`(historyStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))
        `when`(bookmarkStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))

        val migrator = FennecMigrator.Builder(testContext)
            .migrateHistory(historyStorage)
            .migrateBookmarks(bookmarkStorage)
            .setCoroutineContext(this.coroutineContext)
            .setProfile(FennecProfile(
                "test",
                File(getTestPath("combined"),
                    "basic"
                ).absolutePath,
                true
            ))
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(2, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertFalse(this.getValue(Migration.History).success)
            assertFalse(this.getValue(Migration.Bookmarks).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync().await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 6`() = runBlocking {
        // Open tabs migration without configured path to sessions.
        val migrator = FennecMigrator.Builder(testContext)
            .migrateOpenTabs(mock())
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.OpenTabs))
            assertFalse(this.getValue(Migration.OpenTabs).success)
        }
    }
}