/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.sync.AccessTokenInfo
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthExceptionType
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.OAuthScopedKey
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.Event
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.authErrorRegistry
import mozilla.components.service.fxa.manager.SCOPE_SYNC
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.fxa.sharing.ShareableAuthInfo
import mozilla.components.service.fxa.sync.SyncManager
import mozilla.components.service.fxa.sync.SyncDispatcher
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.lang.Exception
import java.lang.IllegalArgumentException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.CoroutineContext

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
open class TestableFxaAccountManager(
    context: Context,
    config: ServerConfig,
    private val storage: AccountStorage,
    capabilities: Set<DeviceCapability> = emptySet(),
    syncConfig: SyncConfig? = null,
    coroutineContext: CoroutineContext,
    private val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, DeviceConfig("test", DeviceType.UNKNOWN, capabilities), syncConfig, emptySet(), coroutineContext) {
    override fun createAccount(config: ServerConfig): OAuthAccount {
        return block()
    }

    override fun getAccountStorage(): AccountStorage {
        return storage
    }
}

@RunWith(AndroidJUnit4::class)
class FxaAccountManagerTest {

    @After
    fun cleanup() {
        // This registry is global, so we need to clear it between test runs, otherwise different
        // manager instances will be kept around.
        authErrorRegistry.unregisterObservers()
        SyncAuthInfoCache(testContext).clear()
    }

    @Test
    fun `state transitions`() {
        // State 'Start'.
        var state = AccountState.Start

        assertEquals(AccountState.Start, FxaAccountManager.nextState(state, Event.Init))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.AccountNotFound))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.AccountRestored))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticate))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticated("code", "state")))
        assertNull(FxaAccountManager.nextState(state, Event.FetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FetchedProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToAuthenticate))
        assertNull(FxaAccountManager.nextState(state, Event.Logout))
        assertNull(FxaAccountManager.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaAccountManager.nextState(state, Event.SignInShareableAccount(mock())))
        assertNull(FxaAccountManager.nextState(state, Event.SignedInShareableAccount))

        // State 'NotAuthenticated'.
        state = AccountState.NotAuthenticated
        assertNull(FxaAccountManager.nextState(state, Event.Init))
        assertNull(FxaAccountManager.nextState(state, Event.AccountNotFound))
        assertNull(FxaAccountManager.nextState(state, Event.AccountRestored))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.Authenticate))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.Pair("auth://pair")))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.Authenticated("code", "state")))
        assertNull(FxaAccountManager.nextState(state, Event.FetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FetchedProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.FailedToAuthenticate))
        assertNull(FxaAccountManager.nextState(state, Event.Logout))
        assertNull(FxaAccountManager.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.SignInShareableAccount(mock())))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.SignedInShareableAccount))

        // State 'AuthenticatedNoProfile'.
        state = AccountState.AuthenticatedNoProfile
        assertNull(FxaAccountManager.nextState(state, Event.Init))
        assertNull(FxaAccountManager.nextState(state, Event.AccountNotFound))
        assertNull(FxaAccountManager.nextState(state, Event.AccountRestored))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticate))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticated("code", "state")))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.FetchProfile))
        assertEquals(AccountState.AuthenticatedWithProfile, FxaAccountManager.nextState(state, Event.FetchedProfile))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.Logout))
        assertEquals(AccountState.AuthenticationProblem, FxaAccountManager.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaAccountManager.nextState(state, Event.SignInShareableAccount(mock())))
        assertNull(FxaAccountManager.nextState(state, Event.SignedInShareableAccount))

        // State 'AuthenticatedWithProfile'.
        state = AccountState.AuthenticatedWithProfile
        assertNull(FxaAccountManager.nextState(state, Event.Init))
        assertNull(FxaAccountManager.nextState(state, Event.AccountNotFound))
        assertNull(FxaAccountManager.nextState(state, Event.AccountRestored))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticate))
        assertNull(FxaAccountManager.nextState(state, Event.Authenticated("code", "state")))
        assertNull(FxaAccountManager.nextState(state, Event.FetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FetchedProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToFetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.Logout))
        assertEquals(AccountState.AuthenticationProblem, FxaAccountManager.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaAccountManager.nextState(state, Event.SignInShareableAccount(mock())))
        assertNull(FxaAccountManager.nextState(state, Event.SignedInShareableAccount))

        // State 'AuthenticationProblem'.
        state = AccountState.AuthenticationProblem
        assertNull(FxaAccountManager.nextState(state, Event.Init))
        assertNull(FxaAccountManager.nextState(state, Event.AccountNotFound))
        assertNull(FxaAccountManager.nextState(state, Event.AccountRestored))
        assertEquals(AccountState.AuthenticationProblem, FxaAccountManager.nextState(state, Event.Authenticate))
        assertEquals(AccountState.AuthenticatedNoProfile, FxaAccountManager.nextState(state, Event.Authenticated("code", "state")))
        assertNull(FxaAccountManager.nextState(state, Event.FetchProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FetchedProfile))
        assertNull(FxaAccountManager.nextState(state, Event.FailedToFetchProfile))
        assertEquals(AccountState.AuthenticationProblem, FxaAccountManager.nextState(state, Event.FailedToAuthenticate))
        assertEquals(AccountState.NotAuthenticated, FxaAccountManager.nextState(state, Event.Logout))
        assertNull(FxaAccountManager.nextState(state, Event.AuthenticationError(AuthException(AuthExceptionType.UNAUTHORIZED))))
        assertNull(FxaAccountManager.nextState(state, Event.SignInShareableAccount(mock())))
        assertNull(FxaAccountManager.nextState(state, Event.SignedInShareableAccount))
    }

    class TestSyncDispatcher(registry: ObserverRegistry<SyncStatusObserver>) : SyncDispatcher, Observable<SyncStatusObserver> by registry {
        val inner: SyncDispatcher = mock()
        override fun isSyncActive(): Boolean {
            return inner.isSyncActive()
        }

        override fun syncNow(startup: Boolean) {
            inner.syncNow(startup)
        }

        override fun startPeriodicSync(unit: TimeUnit, period: Long) {
            inner.startPeriodicSync(unit, period)
        }

        override fun stopPeriodicSync() {
            inner.stopPeriodicSync()
        }

        override fun workersStateChanged(isRunning: Boolean) {
            inner.workersStateChanged(isRunning)
        }

        override fun close() {
            inner.close()
        }
    }

    class TestSyncManager(config: SyncConfig) : SyncManager(config) {
        val dispatcherRegistry = ObserverRegistry<SyncStatusObserver>()
        val dispatcher: TestSyncDispatcher = TestSyncDispatcher(dispatcherRegistry)

        private var dispatcherUpdatedCount = 0
        override fun createDispatcher(stores: Set<String>): SyncDispatcher {
            return dispatcher
        }

        override fun dispatcherUpdated(dispatcher: SyncDispatcher) {
            dispatcherUpdatedCount++
        }
    }

    class TestSyncStatusObserver : SyncStatusObserver {
        var onStartedCount = 0
        var onIdleCount = 0
        var onErrorCount = 0

        override fun onStarted() {
            onStartedCount++
        }

        override fun onIdle() {
            onIdleCount++
        }

        override fun onError(error: Exception?) {
            onErrorCount++
        }
    }

    @Test
    fun `updating sync config, without it at first`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()

        val syncAccessTokenExpiresAt = System.currentTimeMillis() + 10 * 60 * 1000L
        val account = StatePersistenceTestableAccount(profile, constellation, tokenServerEndpointUrl = "https://some.server.com/test") {
            AccessTokenInfo(
                SCOPE_SYNC,
                "tolkien",
                OAuthScopedKey("kty-test", SCOPE_SYNC, "kid-test", "k-test"),
                syncAccessTokenExpiresAt
            )
        }

        var latestSyncManager: TestSyncManager? = null
        // Without sync config to begin with. NB: we're pretending that we "have" a sync scope.
        val manager = object : TestableFxaAccountManager(
            context = testContext,
            config = ServerConfig.release("dummyId", "http://auth-url/redirect"),
            storage = accountStorage,
            capabilities = setOf(DeviceCapability.SEND_TAB),
            syncConfig = null,
            coroutineContext = this@runBlocking.coroutineContext,
            block = { account }
        ) {
            override fun createSyncManager(config: SyncConfig): SyncManager {
                return TestSyncManager(config).also { latestSyncManager = it }
            }
        }

        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(CompletableDeferred(true))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        manager.initAsync().await()

        // Can check if sync is running (it's not!), even if we don't have sync configured.
        assertFalse(manager.isSyncActive())

        val syncStatusObserver = TestSyncStatusObserver()
        val lifecycleOwner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(lifecycleOwner.lifecycle).thenReturn(lifecycle)
        manager.registerForSyncEvents(syncStatusObserver, lifecycleOwner, true)

        // Bad configuration: no stores.
        try {
            manager.setSyncConfigAsync(SyncConfig(setOf())).await()
            fail()
        } catch (e: IllegalArgumentException) {}

        assertNull(latestSyncManager)

        assertEquals(0, syncStatusObserver.onStartedCount)
        assertEquals(0, syncStatusObserver.onIdleCount)
        assertEquals(0, syncStatusObserver.onErrorCount)

        // No periodic sync.
        manager.setSyncConfigAsync(SyncConfig(setOf("history"))).await()

        assertNotNull(latestSyncManager)
        assertNotNull(latestSyncManager?.dispatcher)
        assertNotNull(latestSyncManager?.dispatcher?.inner)
        verify(latestSyncManager!!.dispatcher.inner, never()).startPeriodicSync(any(), anyLong())
        verify(latestSyncManager!!.dispatcher.inner, never()).stopPeriodicSync()
        verify(latestSyncManager!!.dispatcher.inner, times(1)).syncNow(anyBoolean())

        // With periodic sync.
        manager.setSyncConfigAsync(SyncConfig(setOf("history"), 60 * 1000L)).await()

        verify(latestSyncManager!!.dispatcher.inner, times(1)).startPeriodicSync(any(), anyLong())
        verify(latestSyncManager!!.dispatcher.inner, never()).stopPeriodicSync()
        verify(latestSyncManager!!.dispatcher.inner, times(1)).syncNow(anyBoolean())

        // Make sure sync status listeners are working.
        // TODO fix these tests.
//        // Test dispatcher -> sync manager -> account manager -> our test observer.
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onStarted() }
//        assertEquals(1, syncStatusObserver.onStartedCount)
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onIdle() }
//        assertEquals(1, syncStatusObserver.onIdleCount)
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onError(null) }
//        assertEquals(1, syncStatusObserver.onErrorCount)

        // Make sure that sync access token was cached correctly.
        assertFalse(SyncAuthInfoCache(testContext).expired())
        val cachedAuthInfo = SyncAuthInfoCache(testContext).getCached()
        assertNotNull(cachedAuthInfo)
        assertEquals("tolkien", cachedAuthInfo!!.fxaAccessToken)
        assertEquals(syncAccessTokenExpiresAt, cachedAuthInfo.fxaAccessTokenExpiresAt)
        assertEquals("https://some.server.com/test", cachedAuthInfo.tokenServerUrl)
        assertEquals("kid-test", cachedAuthInfo.kid)
        assertEquals("k-test", cachedAuthInfo.syncKey)
    }

    @Test
    fun `updating sync config, with one to begin with`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()

        val syncAccessTokenExpiresAt = System.currentTimeMillis() + 10 * 60 * 1000L
        val account = StatePersistenceTestableAccount(profile, constellation, tokenServerEndpointUrl = "https://some.server.com/test") {
            AccessTokenInfo(
                SCOPE_SYNC,
                "arda",
                OAuthScopedKey("kty-test", SCOPE_SYNC, "kid-test", "k-test"),
                syncAccessTokenExpiresAt
            )
        }

        // With a sync config this time.
        var latestSyncManager: TestSyncManager? = null
        val syncConfig = SyncConfig(setOf("history"), syncPeriodInMinutes = 120L)
        val manager = object : TestableFxaAccountManager(
            context = testContext,
            config = ServerConfig.release("dummyId", "http://auth-url/redirect"),
            storage = accountStorage,
            capabilities = setOf(DeviceCapability.SEND_TAB),
            syncConfig = syncConfig,
            coroutineContext = this@runBlocking.coroutineContext,
            block = { account }
        ) {
            override fun createSyncManager(config: SyncConfig): SyncManager {
                return TestSyncManager(config).also { latestSyncManager = it }
            }
        }

        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(CompletableDeferred(true))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        val syncStatusObserver = TestSyncStatusObserver()
        val lifecycleOwner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(lifecycleOwner.lifecycle).thenReturn(lifecycle)
        manager.registerForSyncEvents(syncStatusObserver, lifecycleOwner, true)

        manager.initAsync().await()

        // Make sure that sync access token was cached correctly.
        assertFalse(SyncAuthInfoCache(testContext).expired())
        val cachedAuthInfo = SyncAuthInfoCache(testContext).getCached()
        assertNotNull(cachedAuthInfo)
        assertEquals("arda", cachedAuthInfo!!.fxaAccessToken)
        assertEquals(syncAccessTokenExpiresAt, cachedAuthInfo.fxaAccessTokenExpiresAt)
        assertEquals("https://some.server.com/test", cachedAuthInfo.tokenServerUrl)
        assertEquals("kid-test", cachedAuthInfo.kid)
        assertEquals("k-test", cachedAuthInfo.syncKey)

        // Make sure periodic syncing started, and an immediate sync was requested.
        assertNotNull(latestSyncManager)
        assertNotNull(latestSyncManager!!.dispatcher)
        assertNotNull(latestSyncManager!!.dispatcher.inner)
        verify(latestSyncManager!!.dispatcher.inner, times(1)).startPeriodicSync(any(), anyLong())
        verify(latestSyncManager!!.dispatcher.inner, never()).stopPeriodicSync()
        verify(latestSyncManager!!.dispatcher.inner, times(1)).syncNow(anyBoolean())

        // Can trigger syncs.
        manager.syncNowAsync().await()
        verify(latestSyncManager!!.dispatcher.inner, times(2)).syncNow(anyBoolean())
        manager.syncNowAsync(startup = true).await()
        verify(latestSyncManager!!.dispatcher.inner, times(3)).syncNow(anyBoolean())

        // TODO fix these tests
//        assertEquals(0, syncStatusObserver.onStartedCount)
//        assertEquals(0, syncStatusObserver.onIdleCount)
//        assertEquals(0, syncStatusObserver.onErrorCount)

        // Make sure sync status listeners are working.
//        // Test dispatcher -> sync manager -> account manager -> our test observer.
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onStarted() }
//        assertEquals(1, syncStatusObserver.onStartedCount)
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onIdle() }
//        assertEquals(1, syncStatusObserver.onIdleCount)
//        latestSyncManager!!.dispatcherRegistry.notifyObservers { onError(null) }
//        assertEquals(1, syncStatusObserver.onErrorCount)

        // Turn off periodic syncing.
        manager.setSyncConfigAsync(SyncConfig(setOf("history"))).await()

        verify(latestSyncManager!!.dispatcher.inner, never()).startPeriodicSync(any(), anyLong())
        verify(latestSyncManager!!.dispatcher.inner, never()).stopPeriodicSync()
        verify(latestSyncManager!!.dispatcher.inner, times(1)).syncNow(anyBoolean())

        // Can trigger syncs.
        manager.syncNowAsync().await()
        verify(latestSyncManager!!.dispatcher.inner, times(2)).syncNow(anyBoolean())
        manager.syncNowAsync(startup = true).await()
        verify(latestSyncManager!!.dispatcher.inner, times(3)).syncNow(anyBoolean())

        // Pretend sync is running.
        `when`(latestSyncManager!!.dispatcher.inner.isSyncActive()).thenReturn(true)
        assertTrue(manager.isSyncActive())

        // Pretend sync is not running.
        `when`(latestSyncManager!!.dispatcher.inner.isSyncActive()).thenReturn(false)
        assertFalse(manager.isSyncActive())
    }

    @Test
    fun `migrating an account via migrateAccountAsync`() = runBlocking {
        // We'll test three scenarios:
        // - hitting a network issue during migration
        // - hitting an auth issue during migration (bad credentials)
        // - all good, migrated successfully
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)
        val accountObserver: AccountObserver = mock()

        val manager = TestableFxaAccountManager(
                testContext, ServerConfig.release("dummyId", "http://auth-url/redirect"), accountStorage,
                setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }
        manager.register(accountObserver)

        // We don't have an account at the start.
        `when`(accountStorage.read()).thenReturn(null)
        manager.initAsync().await()

        // Bad package name.
        var migratableAccount = ShareableAccount(
            email = "test@example.com",
            sourcePackage = "org.mozilla.firefox",
            authInfo = ShareableAuthInfo("session", "kSync", "kXCS")
        )

        // TODO Need to mock inputs into - mock a PackageManager, and have it return PackageInfo with the right signature.
//        AccountSharing.isTrustedPackage

        // We failed to migrate for some reason.
        account.ableToMigrate = false
        assertFalse(manager.signInWithShareableAccountAsync(migratableAccount).await())

        assertEquals("session", account.latestMigrateAuthInfo?.sessionToken)
        assertEquals("kSync", account.latestMigrateAuthInfo?.kSync)
        assertEquals("kXCS", account.latestMigrateAuthInfo?.kXCS)

        assertNull(manager.authenticatedAccount())

        // Prepare for a successful migration.
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        // Success.
        account.ableToMigrate = true
        migratableAccount = migratableAccount.copy(
            authInfo = ShareableAuthInfo("session2", "kSync2", "kXCS2")
        )

        assertTrue(manager.signInWithShareableAccountAsync(migratableAccount).await())

        assertEquals("session2", account.latestMigrateAuthInfo?.sessionToken)
        assertEquals("kSync2", account.latestMigrateAuthInfo?.kSync)
        assertEquals("kXCS2", account.latestMigrateAuthInfo?.kXCS)

        assertNotNull(manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        verify(accountObserver, times(1)).onAuthenticated(account, true)
    }

    @Test
    fun `restored account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig.release("dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(CompletableDeferred(true))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)
        manager.initAsync().await()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that ensureCapabilities fired, but not the device initialization (since we're restoring).
        verify(constellation).ensureCapabilitiesAsync(setOf(DeviceCapability.SEND_TAB))
        verify(constellation, never()).initDeviceAsync(any(), any(), any())

        // Assert that periodic account refresh never started.
        // See https://github.com/mozilla-mobile/android-components/issues/3433
        verify(constellation, never()).startPeriodicRefresh()
        // Assert that we cancel any existing periodic jobs.
        verify(constellation).stopPeriodicRefresh()

        // Assert that we refresh device state.
        verify(constellation).refreshDeviceStateAsync()

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `restored account state persistence, ensureCapabilities hit an intermittent error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig.release("dummyId", "http://auth-url/redirect"), accountStorage,
                setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(CompletableDeferred(false))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)
        manager.initAsync().await()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that ensureCapabilities fired, but not the device initialization (since we're restoring).
        verify(constellation).ensureCapabilitiesAsync(setOf(DeviceCapability.SEND_TAB))
        verify(constellation, never()).initDeviceAsync(any(), any(), any())

        // Assert that periodic account refresh never started.
        // See https://github.com/mozilla-mobile/android-components/issues/3433
        verify(constellation, never()).startPeriodicRefresh()

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `restored account state persistence, hit an auth error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation, ableToRecoverFromAuthError = false)

        val accountObserver: AccountObserver = mock()
        val manager = TestableFxaAccountManager(
            testContext, ServerConfig.release("dummyId", "http://auth-url/redirect"), accountStorage,
                setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        manager.register(accountObserver)

        `when`(constellation.ensureCapabilitiesAsync(any())).then {
            // Hit an auth error.
            authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED)) }
            CompletableDeferred(false)
        }
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)

        assertFalse(manager.accountNeedsReauth())
        assertFalse(account.accessTokenErrorCalled)
        verify(accountObserver, never()).onAuthenticationProblems()

        manager.initAsync().await()

        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        assertTrue(account.accessTokenErrorCalled)
    }

    @Test(expected = FxaPanicException::class)
    fun `restored account state persistence, hit an fxa panic which is re-thrown`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val accountObserver: AccountObserver = mock()
        val manager = TestableFxaAccountManager(
            testContext, ServerConfig.release("dummyId", "http://auth-url/redirect"), accountStorage,
                setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        manager.register(accountObserver)

        // Hit a panic while we're restoring account.
        val fxaPanic = CompletableDeferred<Boolean>()
        fxaPanic.completeExceptionally(FxaPanicException("panic!"))
        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(fxaPanic)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)

        assertFalse(manager.accountNeedsReauth())
        verify(accountObserver, never()).onAuthenticationProblems()

        manager.initAsync().await()
    }

    @Test
    fun `newly authenticated account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)
        val accountObserver: AccountObserver = mock()
        // We are not using the "prepareHappy..." helper method here, because our account isn't a mock,
        // but an actual implementation of the interface.
        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage,
                setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        manager.register(accountObserver)

        // Kick it off, we'll get into a "NotAuthenticated" state.
        manager.initAsync().await()

        assertNull(account.persistenceCallback)

        // Perform authentication.

        assertEquals("auth://url", manager.beginAuthenticationAsync().await())

        // Assert that periodic account refresh didn't start after kicking off auth.
        verify(constellation, never()).startPeriodicRefresh()

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that periodic account refresh is never started after finishing auth.
        // See https://github.com/mozilla-mobile/android-components/issues/3433
        verify(constellation, never()).startPeriodicRefresh()

        // Assert that initDevice fired, but not ensureCapabilities (since we're initing a new account).
        verify(constellation).initDeviceAsync(any(), any(), eq(setOf(DeviceCapability.SEND_TAB)))
        verify(constellation, never()).ensureCapabilitiesAsync(any())

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    class StatePersistenceTestableAccount(
        private val profile: Profile,
        private val constellation: DeviceConstellation,
        val ableToRecoverFromAuthError: Boolean = false,
        val tokenServerEndpointUrl: String? = null,
        var ableToMigrate: Boolean = false,
        val accessToken: (() -> AccessTokenInfo)? = null
    ) : OAuthAccount {

        var persistenceCallback: StatePersistenceCallback? = null
        var accessTokenErrorCalled = false
        var latestMigrateAuthInfo: ShareableAuthInfo? = null

        override fun beginOAuthFlowAsync(scopes: Set<String>): Deferred<String?> {
            return CompletableDeferred("auth://url")
        }

        override fun beginPairingFlowAsync(pairingUrl: String, scopes: Set<String>): Deferred<String?> {
            return CompletableDeferred("auth://url")
        }

        override fun getProfileAsync(ignoreCache: Boolean): Deferred<Profile?> {
            return CompletableDeferred(profile)
        }

        override fun getProfileAsync(): Deferred<Profile?> {
            return CompletableDeferred(profile)
        }

        override fun completeOAuthFlowAsync(code: String, state: String): Deferred<Boolean> {
            return CompletableDeferred(true)
        }

        override fun migrateFromSessionTokenAsync(sessionToken: String, kSync: String, kXCS: String): Deferred<Boolean> {
            latestMigrateAuthInfo = ShareableAuthInfo(sessionToken, kSync, kXCS)
            return CompletableDeferred(ableToMigrate)
        }

        override fun getAccessTokenAsync(singleScope: String): Deferred<AccessTokenInfo?> {
            val token = accessToken?.invoke()
            if (token != null) return CompletableDeferred(token)

            fail()
            return CompletableDeferred(null)
        }

        override fun checkAuthorizationStatusAsync(singleScope: String): Deferred<Boolean?> {
            accessTokenErrorCalled = true
            return CompletableDeferred(ableToRecoverFromAuthError)
        }

        override fun getTokenServerEndpointURL(): String {
            if (tokenServerEndpointUrl != null) return tokenServerEndpointUrl

            fail()
            return ""
        }

        override fun registerPersistenceCallback(callback: StatePersistenceCallback) {
            persistenceCallback = callback
        }

        override fun deviceConstellation(): DeviceConstellation {
            return constellation
        }

        override fun disconnectAsync(): Deferred<Boolean> {
            fail()
            return CompletableDeferred(false)
        }

        override fun toJSONString(): String {
            fail()
            return ""
        }

        override fun close() {
            // Only expect 'close' to be called if we can't recover from an auth error.
            if (ableToRecoverFromAuthError) {
                fail()
            }
        }
    }

    @Test
    fun `error reading persisted account`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val readException = FxaException("pretend we failed to parse the account")
        `when`(accountStorage.read()).thenThrow(readException)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig.release("dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        )

        val accountObserver = object : AccountObserver {
            override fun onLoggedOut() {
                fail()
            }

            override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
                fail()
            }

            override fun onAuthenticationProblems() {
                fail()
            }

            override fun onProfileUpdated(profile: Profile) {
                fail()
            }
        }

        manager.register(accountObserver)
        manager.initAsync().await()
    }

    @Test
    fun `no persisted account`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, never()).onLoggedOut()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, never()).clear()

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())
    }

    @Test
    fun `with persisted account and profile`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(
            "testUid", "test@example.com", null, "Test Profile")
        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(mockAccount)
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.ensureCapabilitiesAsync(any())).thenReturn(CompletableDeferred(true))

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage,
                emptySet(), null, this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)

        manager.initAsync().await()

        // Make sure that account and profile observers are fired exactly once.
        verify(accountObserver, times(1)).onAuthenticated(mockAccount, false)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, never()).clear()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        // Assert that we don't refresh device state for non-SEND_TAB enabled devices.
        verify(constellation, never()).refreshDeviceStateAsync()

        // Make sure 'logoutAsync' clears out state and fires correct observers.
        reset(accountObserver)
        reset(accountStorage)
        `when`(mockAccount.disconnectAsync()).thenReturn(CompletableDeferred(true))
        verify(mockAccount, never()).disconnectAsync()
        manager.logoutAsync().await()

        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, times(1)).onLoggedOut()
        verify(mockAccount, times(1)).disconnectAsync()

        verify(accountStorage, never()).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, times(1)).clear()

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())
    }

    @Test
    fun `happy authentication and profile flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, true)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test(expected = FxaPanicException::class)
    fun `fxa panic during initDevice flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val fxaPanic = CompletableDeferred<Boolean>()
        fxaPanic.completeExceptionally(FxaPanicException("panic!"))
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(fxaPanic)

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
    }

    @Test(expected = FxaPanicException::class)
    fun `fxa panic during pairing flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))

        val fxaPanic = CompletableDeferred<String>()
        fxaPanic.completeExceptionally(FxaPanicException("panic!"))

        `when`(mockAccount.beginPairingFlowAsync(any(), any())).thenReturn(fxaPanic)
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = coroutineContext
        ) {
            mockAccount
        }

        manager.initAsync().await()
        manager.beginAuthenticationAsync("http://pairing.com").await()
        fail()
    }

    @Test
    fun `happy pairing authentication and profile flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, true)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `unhappy authentication flow`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountObserver: AccountObserver = mock()
        val manager = prepareUnhappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)

        assertNull(manager.beginAuthenticationAsync().await())

        // Confirm that account state observable doesn't receive authentication errors.
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        assertEquals("auth://url", manager.beginAuthenticationAsync().await())

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, true)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `unhappy pairing authentication flow`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountObserver: AccountObserver = mock()
        val manager = prepareUnhappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)

        assertNull(manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())

        // Confirm that account state observable doesn't receive authentication errors.
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        assertEquals("auth://url", manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, true)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `authentication issues are propagated via AccountObserver`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        // Our recovery flow should attempt to hit this API. Model the "can't recover" condition by returning 'false'.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(false))

        // At this point, we're logged in. Trigger a 401.
        authErrorRegistry.notifyObservers {
            runBlocking(this@runBlocking.coroutineContext) {
                onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED, FxaUnauthorizedException("401"))).await()
            }
        }

        verify(accountObserver, times(1)).onAuthenticationProblems()
        assertTrue(manager.accountNeedsReauth())
        assertEquals(mockAccount, manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Able to re-authenticate.
        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertEquals(mockAccount, manager.authenticatedAccount())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountObserver).onAuthenticated(mockAccount, true)
        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `authentication issues are recoverable via checkAuthorizationState`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        // Recovery flow will hit this API, and will recover if it returns 'true'.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(true))

        // At this point, we're logged in. Trigger a 401.
        authErrorRegistry.notifyObservers {
            runBlocking(this@runBlocking.coroutineContext) {
                onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED, FxaUnauthorizedException("401"))).await()
            }
        }

        // Since we've recovered, outside observers should not have witnessed the momentary problem state.
        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())
        assertEquals(mockAccount, manager.authenticatedAccount())
    }

    @Test
    fun `unhappy profile fetching flow`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))
        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, true)
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Make sure we can re-try fetching a profile. This time, let's have it succeed.
        reset(accountObserver)
        val profile = Profile(
            uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))

        manager.updateProfileAsync().await()

        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onLoggedOut()
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `profile fetching flow hit an unrecoverable auth problem`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        `when`(mockAccount.getProfileAsync(anyBoolean())).then {
            // Hit an auth error.
            authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED)) }
            CompletableDeferred(value = null)
        }

        // Our recovery flow should attempt to hit this API. Model the "can't recover" condition by returning false.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(false))

        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatusAsync(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        verify(mockAccount, times(1)).checkAuthorizationStatusAsync(eq("profile"))
        Unit
    }

    @Test
    fun `profile fetching flow hit an unrecoverable auth problem for which we can't determine a recovery state`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        `when`(mockAccount.getProfileAsync(anyBoolean())).then {
            // Hit an auth error.
            authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED)) }
            CompletableDeferred(value = null)
        }

        // Our recovery flow should attempt to hit this API. Model the "don't know what's up" condition by returning null.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(value = null))

        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatusAsync(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        verify(mockAccount, times(1)).checkAuthorizationStatusAsync(eq("profile"))
        Unit
    }

    @Test
    fun `profile fetching flow hit a recoverable auth problem`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val captor = argumentCaptor<Boolean>()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        val profile = Profile(
                uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        var didFailProfileFetch = false
        `when`(mockAccount.getProfileAsync(anyBoolean())).then {
            // Hit an auth error, but only once. As we recover from it, we'll attempt to fetch a profile
            // again. At that point, we'd like to succeed.
            if (!didFailProfileFetch) {
                didFailProfileFetch = true
                authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED)) }
                CompletableDeferred<Profile?>(value = null)
            } else {
                CompletableDeferred(profile)
            }
        }

        // Recovery flow will hit this API, return a success.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(true))

        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatusAsync(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        assertFalse(manager.accountNeedsReauth())
        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
        verify(accountObserver, never()).onAuthenticationProblems()
        // Once for the initial auth success, then once again after we recover from an auth problem.
        verify(accountObserver, times(2)).onAuthenticated(eq(mockAccount), captor.capture())
        assertTrue(captor.allValues[0])
        assertFalse(captor.allValues[1])
        // Verify that we went through the recovery flow.
        verify(mockAccount, times(1)).checkAuthorizationStatusAsync(eq("profile"))
        Unit
    }

    @Test(expected = FxaPanicException::class)
    fun `profile fetching flow hit an fxa panic, which is re-thrown`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        val exceptionalProfile = CompletableDeferred<Profile>()
        val fxaException = FxaPanicException("500")
        exceptionalProfile.completeExceptionally(fxaException)

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))
        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(exceptionalProfile)
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), anyBoolean())
        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
    }

    private fun prepareHappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver,
        coroutineContext: CoroutineContext
    ): FxaAccountManager {

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = coroutineContext
        ) {
            mockAccount
        }

        manager.register(accountObserver)

        runBlocking(coroutineContext) {
            manager.initAsync().await()
        }

        return manager
    }

    private fun prepareUnhappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver,
        coroutineContext: CoroutineContext
    ): FxaAccountManager {
        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))

        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
                ServerConfig.release("dummyId", "bad://url"),
                accountStorage, coroutineContext = coroutineContext
        ) {
            mockAccount
        }

        manager.register(accountObserver)

        runBlocking(coroutineContext) {
            manager.initAsync().await()
        }

        return manager
    }

    private fun mockDeviceConstellation(): DeviceConstellation {
        val c: DeviceConstellation = mock()
        `when`(c.refreshDeviceStateAsync()).thenReturn(CompletableDeferred(true))
        return c
    }
}