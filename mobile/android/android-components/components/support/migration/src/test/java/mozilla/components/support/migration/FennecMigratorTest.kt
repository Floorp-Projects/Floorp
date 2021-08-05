/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.appservices.places.PlacesException
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.MigrationResult
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import java.io.File
import java.lang.IllegalArgumentException
import java.lang.IllegalStateException

@RunWith(AndroidJUnit4::class)
class FennecMigratorTest {
    @Test
    fun `no-op migration`() = runBlocking {
        val migrator = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .build()

        // Can do this once.
        with(migrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }

        // Can do this all day long!
        with(migrator.migrateAsync(mock()).await()) {
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
    fun `history & bookmark & logins migrations must be done before FxA`() {
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
                .migrateLogins(mock())
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
    fun `is not a fennec install detected`() = runBlocking {
        val historyStore = PlacesHistoryStorage(testContext)

        val migrator1 = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .migrateHistory(lazy { historyStore })
            .build()

        assertTrue(migrator1.isFennecInstallation())
        with(migrator1.migrateAsync(mock()).await()) {
            assertTrue(this.containsKey(Migration.History))
        }

        val migrator2 = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/no-browser.db").absolutePath)
            .migrateHistory(lazy { historyStore })
            .build()

        assertFalse(migrator2.isFennecInstallation())
        with(migrator2.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `migrations versioning basics`() = runBlocking {
        val historyStore = PlacesHistoryStorage(testContext)
        val bookmarksStore = PlacesBookmarksStorage(testContext)
        val topSiteStorage = mock<PinnedSiteStorage>()

        // Clear-up storage layers between test runs.
        historyStore.deleteEverything()
        bookmarksStore.getTree(BookmarkRoot.Mobile.id)?.children?.forEach {
            bookmarksStore.deleteNode(it.guid)
        }

        val migrator = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("combined"), "basic").absolutePath, true
                )
            )
            .migrateHistory(lazy { historyStore })
            .migrateBookmarks(lazy { bookmarksStore }, topSiteStorage)
            .build()

        assertEquals(listOf<String>(), historyStore.getVisited())
        assertTrue(historyStore.getVisited().isEmpty())
        assertTrue(bookmarksStore.searchBookmarks("mozilla").isEmpty())

        verifyNoInteractions(topSiteStorage)

        // Can run once.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(3, this.size)

            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertTrue(this.containsKey(Migration.PinnedSites))

            with(this.getValue(Migration.History)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            with(this.getValue(Migration.Bookmarks)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            with(this.getValue(Migration.PinnedSites)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }
        }

        assertEquals(6, historyStore.getVisited().size)
        assertEquals(4, bookmarksStore.searchBookmarks("mozilla").size)
        verifyNoInteractions(topSiteStorage)

        // Do not run again for the same version.
        with(migrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }

        assertEquals(6, historyStore.getVisited().size)
        assertEquals(4, bookmarksStore.searchBookmarks("mozilla").size)

        // Can add another migration type, and it will be the only one to run.
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = BrowserStore(middleware = listOf(middleware))
        val tabsUseCases = TabsUseCases(store = store)

        val expandedMigrator = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test",
                    File(
                        getTestPath("combined"),
                        "basic"
                    ).absolutePath,
                    true
                )
            )
            .migrateHistory(lazy { historyStore })
            .migrateBookmarks(lazy { bookmarksStore }, topSiteStorage)
            .migrateOpenTabs(tabsUseCases)
            .build()

        with(expandedMigrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.OpenTabs))

            with(this.getValue(Migration.OpenTabs)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            middleware.assertLastAction(TabListAction.RestoreAction::class) { action ->
                assertEquals(6, action.tabs.size)
            }
        }

        // Running this migrator again does nothing.
        with(expandedMigrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `pinned sites migration`() = runBlocking {
        val historyStore = PlacesHistoryStorage(testContext)
        val bookmarksStore = PlacesBookmarksStorage(testContext)
        val topSiteStorage = mock<PinnedSiteStorage>()

        val migrator = FennecMigrator.Builder(testContext, mock())
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("combined"), "basic").absolutePath, true
                )
            )
            .migrateHistory(lazy { historyStore })
            .migrateBookmarks(lazy { bookmarksStore }, topSiteStorage)
            .setBrowserDbPath(File(getTestPath("databases/v39-basic"), "pinnedSites-v39.db").absolutePath)
            .build()

        assertTrue(historyStore.getVisited().isEmpty())
        assertTrue(bookmarksStore.searchBookmarks("mozilla").isEmpty())

        verify(topSiteStorage, never()).addPinnedSite(any(), any(), anyBoolean())

        // Can run once.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(3, this.size)

            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertTrue(this.containsKey(Migration.PinnedSites))

            with(this.getValue(Migration.History)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            with(this.getValue(Migration.Bookmarks)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }

            with(this.getValue(Migration.PinnedSites)) {
                assertTrue(this.success)
                assertEquals(1, this.version)
            }
        }

        assertEquals(5, historyStore.getVisited().size)
        assertEquals(2, bookmarksStore.searchBookmarks("mozilla").size)
        verify(topSiteStorage, times(2)).addPinnedSite(any(), any(), anyBoolean())
        verify(topSiteStorage).addPinnedSite(
            "Featured extensions for Android – Add-ons for Firefox Android (en-US)",
            "https://addons.mozilla.org/en-US/android/collections/4757633/mob/?page=1&collection_sort=-popularity"
        )
        verify(topSiteStorage).addPinnedSite(
            "Internet for people, not profit — Mozilla",
            "https://www.mozilla.org/en-US/"
        )
    }

    @Test
    fun `failing migrations are reported - case 1`() = runBlocking {
        val historyStorage = PlacesHistoryStorage(testContext)

        // DB path is set, but db is corrupt.
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateHistory(lazy { historyStorage })
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/corrupt.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertFalse(this.getValue(Migration.History).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 2`() = runBlocking {
        val historyStorage: PlacesHistoryStorage = mock()

        // Fail during history migration.
        `when`(historyStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))

        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateHistory(lazy { historyStorage })
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .setProfile(
                FennecProfile(
                    "test",
                    File(
                        getTestPath("combined"),
                        "basic"
                    ).absolutePath,
                    true
                )
            )
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertFalse(this.getValue(Migration.History).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 4`() = runBlocking {
        val bookmarkStorage: PlacesBookmarksStorage = mock()
        val historyStorage: PlacesHistoryStorage = mock()

        // Fail during history migration.
        `when`(historyStorage.importFromFennec(any())).thenThrow(PlacesException("test exception"))

        `when`(bookmarkStorage.importFromFennec(any())).thenReturn(
            JSONObject().also {
                it.put("num_total", 25)
                it.put("num_succeeded", 23)
                it.put("num_failed", 2)
                it.put("total_duration", 1245L)
            }
        )

        // DB path is configured, partial success (only history failed).
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateHistory(lazy { historyStorage })
            .migrateBookmarks(lazy { bookmarkStorage })
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test",
                    File(
                        getTestPath("combined"),
                        "basic"
                    ).absolutePath,
                    true
                )
            )
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(2, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertFalse(this.getValue(Migration.History).success)
            assertTrue(this.getValue(Migration.Bookmarks).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync(mock()).await()) {
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
            .migrateHistory(lazy { historyStorage })
            .migrateBookmarks(lazy { bookmarkStorage })
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test",
                    File(
                        getTestPath("combined"),
                        "basic"
                    ).absolutePath,
                    true
                )
            )
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(2, this.size)
            assertTrue(this.containsKey(Migration.History))
            assertTrue(this.containsKey(Migration.Bookmarks))
            assertFalse(this.getValue(Migration.History).success)
            assertFalse(this.getValue(Migration.Bookmarks).success)
        }

        // Doesn't auto-rerun failed migration.
        with(migrator.migrateAsync(mock()).await()) {
            assertTrue(this.isEmpty())
        }
    }

    @Test
    fun `failing migrations are reported - case 6`() = runBlocking {
        // Open tabs migration without configured path to sessions.
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateOpenTabs(mock())
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.OpenTabs))
            assertFalse(this.getValue(Migration.OpenTabs).success)
        }
    }

    @Test
    fun `failing migrations are reported - case 7, corrupt fxa state`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "corrupt-married-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `failing migrations are reported - case 8, unsupported pickle version`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "separated-bad-pickle-version-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `failing migrations are reported - case 8, unsupported state version`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "separated-bad-state-version-v10.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `fxa migration - no account`() = runBlocking {
        // FxA migration without configured path to pickle file (test environment path isn't the same as real path).
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        verifyNoInteractions(accountManager)

        // Does not run FxA migration again.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(0, this.size)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `fxa migration - unauthenticated account`() = runBlocking {
        // FxA migration without configured path to pickle file (test environment path isn't the same as real path).
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "separated-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        verifyNoInteractions(accountManager)

        // Does not run FxA migration again.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(0, this.size)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `fxa migration - authenticated account, sign-in succeeded`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "married-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Success)

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        val captor = argumentCaptor<ShareableAccount>()
        verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

        assertEquals("test@example.com", captor.value.email)
        // This is going to be package name (org.mozilla.firefox) in actual builds.
        assertEquals("mozilla.components.support.migration.test", captor.value.sourcePackage)
        assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
        assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
        assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)

        // Does not run FxA migration again.
        reset(accountManager)
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(0, this.size)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `fxa migration - authenticated account, sign-in will retry`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "married-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.WillRetry)

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertTrue(this.getValue(Migration.FxA).success)
        }

        val captor = argumentCaptor<ShareableAccount>()
        verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

        assertEquals("test@example.com", captor.value.email)
        // This is going to be package name (org.mozilla.firefox) in actual builds.
        assertEquals("mozilla.components.support.migration.test", captor.value.sourcePackage)
        assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
        assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
        assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)

        // Does not run FxA migration again.
        reset(accountManager)
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(0, this.size)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `fxa migration - authenticated account, sign-in failed`() = runBlocking {
        val accountManager: FxaAccountManager = mock()
        val migrator = FennecMigrator.Builder(testContext, mock())
            .migrateFxa(lazy { accountManager })
            .setFxaState(File(getTestPath("fxa"), "cohabiting-v4.json"))
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        // For now, we don't treat sign-in failure any different from success. E.g. it's a one-shot attempt.
        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Failure)

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.FxA))
            assertFalse(this.getValue(Migration.FxA).success)
        }

        val captor = argumentCaptor<ShareableAccount>()
        verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

        assertEquals("test@example.com", captor.value.email)
        // This is going to be package name (org.mozilla.firefox) in actual builds.
        assertEquals("mozilla.components.support.migration.test", captor.value.sourcePackage)
        assertEquals("252bc4ccc3a239fsdfsdf32fg32wf3w4e3472d41d1a204890", captor.value.authInfo.kSync)
        assertEquals("0b3ba79b18bd9fsdfsdf4g234adedd87", captor.value.authInfo.kXCS)
        assertEquals("fsdfjsdffsdf342f23g3ogou97328uo23ij", captor.value.authInfo.sessionToken)

        // Does not run FxA migration again.
        reset(accountManager)
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(0, this.size)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `logins migrations - no master password`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key").also {
            it.wipeLocal()
        }
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "basic").absolutePath, true
                )
            )
            .setKey4DbName("key4.db")
            .setSignonsDbName("signons.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertTrue(this.getValue(Migration.Logins).success)
        }

        with(loginStorage.list()) {
            assertEquals(2, this.size)
            with(this[0]) {
                assertEquals("https://getbootstrap.com", origin)
                assertEquals("test@example.com", username)
                assertEquals("super duper pass 1", password)
                assertEquals(null, httpRealm)
                assertEquals("https://getbootstrap.com", formActionOrigin)
                assertEquals(1, timesUsed)
                assertEquals(1574735368749, timeCreated)
                assertEquals(1574735368749, timeLastUsed)
                assertEquals(1574735368749, timePasswordChanged)
                assertEquals("", usernameField)
                assertEquals("", passwordField)
            }
            with(this[1]) {
                assertEquals("https://html.com", origin)
                assertEquals("", username)
                assertEquals("testp", password)
                assertEquals(null, httpRealm)
                assertEquals("https://html.com", formActionOrigin)
                assertEquals(1, timesUsed)
                assertEquals(1574735237274, timeCreated)
                assertEquals(1574735237274, timeLastUsed)
                assertEquals(1574735237274, timePasswordChanged)
                assertEquals("", usernameField)
                assertEquals("password", passwordField)
            }
        }
    }

    @Test
    fun `logins migrations - with master password`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key")
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "with-mp").absolutePath, true
                )
            )
            .setKey4DbName("key4.db")
            .setSignonsDbName("signons.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        // Expect MP presence to be detected, and migration to succeed. But, not actual records are imported.
        // We actually _can_ migrate with a set MP, but that's currently exposed over the FennecMigrator interface.
        // See FennecLoginsMigration for details.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertTrue(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `logins migrations - with mp and empty key4db`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key").also {
            it.wipeLocal()
        }
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "with-mp").absolutePath, true
                )
            )
            .setKey4DbName("empty-key4.db")
            .setSignonsDbName("signons.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        // Empty key4db means we can't decrypt records, or even check for MP status.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertFalse(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `logins migrations - with mp and no nss in key4db`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key")
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "with-mp").absolutePath, true
                )
            )
            .setKey4DbName("noNss-key4.db")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .setSignonsDbName("signons.sqlite")
            .build()

        // Empty nss table in key4db means we can't decrypt records, but we still check the master password.
        // It's detected, and migration aborted.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertTrue(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `logins migrations - missing profile`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key").also {
            it.wipeLocal()
        }
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setKey4DbName("noNss-key4.db")
            .setSignonsDbName("signons.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        // Empty nss table in key4db means we can't decrypt records, but we still check the master password.
        // It's detected, and migration aborted.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertFalse(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `logins migrations - with master password, old signons version`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key")
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "with-mp").absolutePath, true
                )
            )
            .setKey4DbName("key4.db")
            .setSignonsDbName("signons-v5.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        // MP is checked first, so we don't even get to check signons version.
        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertTrue(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `logins migrations - without master password, old signons version`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val loginStorage = SyncableLoginsStorage(testContext, "test key")
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateLogins(lazy { loginStorage })
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("logins"), "basic").absolutePath, true
                )
            )
            .setKey4DbName("key4.db")
            .setSignonsDbName("signons-v5.sqlite")
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Logins))
            assertFalse(this.getValue(Migration.Logins).success)
        }

        assertEquals(0, loginStorage.list().size)
    }

    @Test
    fun `settings migration - no fennec prefs`() = runBlocking {
        // Fennec SharedPreferences are missing / empty
        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateSettings()
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Settings))
            assertTrue(this.getValue(Migration.Settings).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `settings migration - missing FHR value`() = runBlocking {
        val fennecAppPrefs = testContext.getSharedPreferences(FennecSettingsMigration.FENNEC_APP_SHARED_PREFS_NAME, Context.MODE_PRIVATE)

        // Make prefs non-empty.
        fennecAppPrefs.edit().putString("dummy", "key").apply()

        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateSettings()
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Settings))
            assertFalse(this.getValue(Migration.Settings).success)
        }
        val captor = argumentCaptor<Exception>()
        verify(crashReporter).submitCaughtException(captor.capture())

        assertEquals(SettingsMigrationException::class, captor.value::class)
        assertEquals("Missing FHR pref value", captor.value.message)
    }

    @Test
    fun `addon migration - no addons installed`() = runBlocking {
        val addonUpdater: AddonUpdater = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val engine: Engine = mock()
        val callbackCaptor = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(callbackCaptor.capture(), any())).thenAnswer {
            callbackCaptor.value.invoke(emptyList())
        }

        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateAddons(engine, addonCollectionProvider, addonUpdater)
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Addons))
            assertTrue(this.getValue(Migration.Addons).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `addon migration - successful migration`() = runBlocking {
        val addon1: WebExtension = mock()
        val addon2: WebExtension = mock()

        val addonUpdater: AddonUpdater = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val engine: Engine = mock()
        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2))
        }

        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        whenever(engine.disableWebExtension(any(), any(), disableSuccessCallback.capture(), any())).thenAnswer {
            disableSuccessCallback.value.invoke(mock())
        }

        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateAddons(engine, addonCollectionProvider, addonUpdater)
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Addons))
            assertTrue(this.getValue(Migration.Addons).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `addon migration - failed to query installed addons`() = runBlocking {
        val addonUpdater: AddonUpdater = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val engine: Engine = mock()
        val errorCallback = argumentCaptor<((Throwable) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(any(), errorCallback.capture())).thenAnswer {
            errorCallback.value.invoke(IllegalArgumentException())
        }

        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateAddons(engine, addonCollectionProvider, addonUpdater)
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Addons))
            assertFalse(this.getValue(Migration.Addons).success)
        }

        val captor = argumentCaptor<Exception>()
        verify(crashReporter).submitCaughtException(captor.capture())

        assertEquals(AddonMigrationException::class, captor.value::class)
        assertEquals("Failed to query installed add-ons: ${IllegalArgumentException::class.java.name}", captor.value.message)
    }

    @Test
    fun `addon migration - failed to migrate some addons`() = runBlocking {
        val addon1: WebExtension = mock()
        val addon2: WebExtension = mock()
        val addon3: WebExtension = mock()
        val addon4: WebExtension = mock()

        val addonUpdater: AddonUpdater = mock()
        val addonCollectionProvider: AddonCollectionProvider = mock()
        val engine: Engine = mock()
        val listSuccessCallback = argumentCaptor<((List<WebExtension>) -> Unit)>()
        whenever(engine.listInstalledWebExtensions(listSuccessCallback.capture(), any())).thenAnswer {
            listSuccessCallback.value.invoke(listOf(addon1, addon2, addon3, addon4))
        }

        val addonCaptor = argumentCaptor<WebExtension>()
        val disableSuccessCallback = argumentCaptor<((WebExtension) -> Unit)>()
        val disableErrorCallback = argumentCaptor<((Throwable) -> Unit)>()
        whenever(
            engine.disableWebExtension(
                addonCaptor.capture(),
                any(),
                disableSuccessCallback.capture(),
                disableErrorCallback.capture()
            )
        )
            .thenAnswer {
                if (addonCaptor.value == addon2 || addonCaptor.value == addon4) {
                    disableErrorCallback.value.invoke(IllegalArgumentException())
                } else {
                    disableSuccessCallback.value.invoke(mock())
                }
            }

        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateAddons(engine, addonCollectionProvider, addonUpdater)
            .setCoroutineContext(this.coroutineContext)
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Addons))
            assertFalse(this.getValue(Migration.Addons).success)
        }

        val captor = argumentCaptor<Exception>()
        // We don't submit duplicate exceptions to Sentry, so should only have one invocation here
        verify(crashReporter, times(1)).submitCaughtException(captor.capture())
        assertEquals(FennecMigratorException.MigrateAddonsException::class, captor.value::class)
        assertEquals(IllegalArgumentException::class, captor.value.cause!!::class)
    }

    @Test
    fun `gecko migration - no prefs_js`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateGecko()
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", getTestPath("empty").absolutePath, true
                )
            )
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Gecko))
            assertTrue(this.getValue(Migration.Gecko).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `gecko migration - invalid prefs_js removed`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateGecko()
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("prefs"), "invalid_fennec_migrator").absolutePath, true
                )
            )
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Gecko))
            assertTrue(this.getValue(Migration.Gecko).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `gecko migration - prefs_js migrated`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateGecko()
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("prefs"), "migrate_fennec_migrator").absolutePath, true
                )
            )
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Gecko))
            assertTrue(this.getValue(Migration.Gecko).success)
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `gecko migration - prefs_js no prefs to migrate`() = runBlocking {
        val crashReporter: CrashReporting = mock()
        val migrator = FennecMigrator.Builder(testContext, crashReporter)
            .migrateGecko()
            .setCoroutineContext(this.coroutineContext)
            .setProfile(
                FennecProfile(
                    "test", File(getTestPath("prefs"), "noaddons_fennec_migrator").absolutePath, true
                )
            )
            .setBrowserDbPath(File(getTestPath("combined"), "basic/browser.db").absolutePath)
            .build()

        with(migrator.migrateAsync(mock()).await()) {
            assertEquals(1, this.size)
            assertTrue(this.containsKey(Migration.Gecko))
            assertTrue(this.getValue(Migration.Gecko).success)
        }
        verifyNoInteractions(crashReporter)
    }
}
