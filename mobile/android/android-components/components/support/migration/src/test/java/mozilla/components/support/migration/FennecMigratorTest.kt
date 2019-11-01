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
import mozilla.components.service.fxa.manager.FxaAccountManager
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
import org.mockito.Mockito.verifyZeroInteractions
import java.io.File
import java.lang.IllegalStateException
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.support.test.argumentCaptor

@RunWith(AndroidJUnit4::class)
class FennecMigratorTest {
    @Test
    fun `no-op migration`() = runBlocking {
        val migrator = FennecMigrator.Builder(testContext, mock())
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
    fun `history migration must be done before bookmarks`() {
        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateBookmarks(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}

        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateBookmarks(mock())
                .migrateHistory(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}
    }

    @Test
    fun `history & bookmark migrations must be done before FxA`() {
        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateFxa(mock())
                .migrateHistory(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}

        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateFxa(mock())
                .migrateBookmarks(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}

        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateFxa(mock())
                .migrateHistory(mock())
                .migrateBookmarks(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}

        try {
            FennecMigrator.Builder(testContext, mock())
                .migrateHistory(mock())
                .migrateFxa(mock())
                .migrateBookmarks(mock())
                .build()
            fail()
        } catch (e: IllegalStateException) {}
    }

    @Test
    fun `migrations versioning basics`() = runBlocking {
        val historyStore = PlacesHistoryStorage(testContext)
        val bookmarksStore = PlacesBookmarksStorage(testContext)

        val migrator = FennecMigrator.Builder(testContext, mock())
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
        val expandedMigrator = FennecMigrator.Builder(testContext, mock())
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
        val migrator = FennecMigrator.Builder(testContext, mock())
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

        val migrator = FennecMigrator.Builder(testContext, mock())
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
        val migrator = FennecMigrator.Builder(testContext, mock())
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
        val migrator = FennecMigrator.Builder(testContext, mock())
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

        val migrator = FennecMigrator.Builder(testContext, mock())
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
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateOpenTabs(mock())
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.OpenTabs))
            assertFalse(this.getValue(Migration.OpenTabs).success)
        }
    }

    @Test
    fun `failing migrations are reported - case 7, corrupt fxa state`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "corrupt-married-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `failing migrations are reported - case 8, unsupported pickle version`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "separated-bad-pickle-version-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `failing migrations are reported - case 8, unsupported state version`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "separated-bad-state-version-v10.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `fxa migration - no account`() = runBlocking {
        // FxA migration without configured path to pickle file (test environment path isn't the same as real path).
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `fxa migration - unauthenticated account`() = runBlocking {
        // FxA migration without configured path to pickle file (test environment path isn't the same as real path).
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "separated-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        verifyZeroInteractions(accountManager)
    }

    @Test
    fun `fxa migration - authenticated account, sign-in succeeded`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "married-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        `when`(accountManager.signInWithShareableAccountAsync(any())).thenReturn(CompletableDeferred(true))

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        val captor = argumentCaptor<ShareableAccount>()
        verify(accountManager).signInWithShareableAccountAsync(captor.capture())

        assertEquals("test@example.com", captor.value.email)
        // This is going to be package name (org.mozilla.firefox) in actual builds.
        assertEquals("mozilla.components.support.migration.test", captor.value.sourcePackage)
        assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
        assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
        assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)
    }

    @Test
    fun `fxa migration - authenticated account, sign-in failed`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(accountManager)
            .setFxaState(File(getTestPath("fxa"), "cohabiting-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .build()

        // For now, we don't treat sign-in failure any different from success. E.g. it's a one-shot attempt.
        `when`(accountManager.signInWithShareableAccountAsync(any())).thenReturn(CompletableDeferred(false))

        with(migrator.migrateAsync().await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        val captor = argumentCaptor<ShareableAccount>()
        verify(accountManager).signInWithShareableAccountAsync(captor.capture())

        assertEquals("test@example.com", captor.value.email)
        // This is going to be package name (org.mozilla.firefox) in actual builds.
        assertEquals("mozilla.components.support.migration.test", captor.value.sourcePackage)
        assertEquals("252bc4ccc3a239fsdfsdf32fg32wf3w4e3472d41d1a204890", captor.value.authInfo.kSync)
        assertEquals("0b3ba79b18bd9fsdfsdf4g234adedd87", captor.value.authInfo.kXCS)
        assertEquals("fsdfjsdffsdf342f23g3ogou97328uo23ij", captor.value.authInfo.sessionToken)
    }
}