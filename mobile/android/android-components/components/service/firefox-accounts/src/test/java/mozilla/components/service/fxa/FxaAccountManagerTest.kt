/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.AccessTokenInfo
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConfig
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.InFlightMigrationState
import mozilla.components.concept.sync.MigratingAccountInfo
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.OAuthScopedKey
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.ServiceResult
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.GlobalAccountManager
import mozilla.components.service.fxa.manager.MigrationResult
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.fxa.sync.SyncDispatcher
import mozilla.components.service.fxa.sync.SyncManager
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
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
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.verifyNoMoreInteractions
import java.util.concurrent.TimeUnit
import kotlin.coroutines.CoroutineContext

internal class TestableStorageWrapper(
    manager: FxaAccountManager,
    accountEventObserverRegistry: ObserverRegistry<AccountEventsObserver>,
    serverConfig: ServerConfig,
    private val block: () -> OAuthAccount = {
        val account: OAuthAccount = mock()
        `when`(account.deviceConstellation()).thenReturn(mock())
        account
    }
) : StorageWrapper(manager, accountEventObserverRegistry, serverConfig) {
    override fun obtainAccount(): OAuthAccount = block()
}

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
internal open class TestableFxaAccountManager(
    context: Context,
    config: ServerConfig,
    private val storage: AccountStorage,
    capabilities: Set<DeviceCapability> = emptySet(),
    syncConfig: SyncConfig? = null,
    coroutineContext: CoroutineContext,
    crashReporter: CrashReporting? = null,
    block: () -> OAuthAccount = {
        val account: OAuthAccount = mock()
        `when`(account.deviceConstellation()).thenReturn(mock())
        account
    }
) : FxaAccountManager(context, config, DeviceConfig("test", DeviceType.UNKNOWN, capabilities), syncConfig, emptySet(), crashReporter, coroutineContext) {
    private val testableStorageWrapper = TestableStorageWrapper(this, accountEventObserverRegistry, serverConfig, block)
    override fun getStorageWrapper(): StorageWrapper {
        return testableStorageWrapper
    }

    override fun getAccountStorage(): AccountStorage {
        return storage
    }

    override fun createSyncManager(config: SyncConfig): SyncManager = mock()
}

const val EXPECTED_AUTH_STATE = "goodAuthState"
const val UNEXPECTED_AUTH_STATE = "badAuthState"

@RunWith(AndroidJUnit4::class)
class FxaAccountManagerTest {

    @After
    fun cleanup() {
        SyncAuthInfoCache(testContext).clear()
        SyncEnginesStorage(testContext).clear()
    }

    internal class TestSyncDispatcher(registry: ObserverRegistry<SyncStatusObserver>) : SyncDispatcher, Observable<SyncStatusObserver> by registry {
        val inner: SyncDispatcher = mock()
        override fun isSyncActive(): Boolean {
            return inner.isSyncActive()
        }

        override fun syncNow(reason: SyncReason, debounce: Boolean) {
            inner.syncNow(reason, debounce)
        }

        override fun startPeriodicSync(unit: TimeUnit, period: Long, initialDelay: Long) {
            inner.startPeriodicSync(unit, period, initialDelay)
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

    internal class TestSyncManager(config: SyncConfig) : SyncManager(config) {
        val dispatcherRegistry = ObserverRegistry<SyncStatusObserver>()
        val dispatcher: TestSyncDispatcher = TestSyncDispatcher(dispatcherRegistry)

        private var dispatcherUpdatedCount = 0
        override fun createDispatcher(supportedEngines: Set<SyncEngine>): SyncDispatcher {
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
    fun `migrating an account via copyAccountAsync - creating a new session token`() = runBlocking {
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
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }
        manager.register(accountObserver)

        // We don't have an account at the start.
        `when`(accountStorage.read()).thenReturn(null)
        manager.start()

        // Bad package name.
        var migratableAccount = ShareableAccount(
            email = "test@example.com",
            sourcePackage = "org.mozilla.firefox",
            authInfo = MigratingAccountInfo("session", "kSync", "kXCS")
        )

        // TODO Need to mock inputs into - mock a PackageManager, and have it return PackageInfo with the right signature.
//        AccountSharing.isTrustedPackage

        // We failed to migrate for some reason.
        account.migrationResult = MigrationResult.Failure
        assertEquals(
            MigrationResult.Failure,
            manager.migrateFromAccount(migratableAccount)
        )

        assertEquals("session", account.latestMigrateAuthInfo!!.sessionToken)
        assertEquals("kSync", account.latestMigrateAuthInfo!!.kSync)
        assertEquals("kXCS", account.latestMigrateAuthInfo!!.kXCS)

        assertNull(manager.authenticatedAccount())

        // Prepare for a successful migration.
        `when`(constellation.finalizeDevice(eq(AuthType.MigratedCopy), any())).thenReturn(ServiceResult.Ok)

        // Success.
        account.migrationResult = MigrationResult.Success
        migratableAccount = migratableAccount.copy(
            authInfo = MigratingAccountInfo("session2", "kSync2", "kXCS2")
        )

        assertEquals(
            MigrationResult.Success,
            manager.migrateFromAccount(migratableAccount)
        )

        assertEquals("session2", account.latestMigrateAuthInfo!!.sessionToken)
        assertEquals("kSync2", account.latestMigrateAuthInfo!!.kSync)
        assertEquals("kXCS2", account.latestMigrateAuthInfo!!.kXCS)

        assertNotNull(manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        verify(constellation, times(1)).finalizeDevice(eq(AuthType.MigratedCopy), any())
        verify(accountObserver, times(1)).onAuthenticated(account, AuthType.MigratedCopy)
    }

    @Test
    fun `migrating an account via migrateAccountAsync - reusing existing session token`() = runBlocking {
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
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }
        manager.register(accountObserver)

        // We don't have an account at the start.
        `when`(accountStorage.read()).thenReturn(null)
        manager.start()

        // Bad package name.
        var migratableAccount = ShareableAccount(
            email = "test@example.com",
            sourcePackage = "org.mozilla.firefox",
            authInfo = MigratingAccountInfo("session", "kSync", "kXCS")
        )

        // TODO Need to mock inputs into - mock a PackageManager, and have it return PackageInfo with the right signature.
//        AccountSharing.isTrustedPackage

        // We failed to migrate for some reason.
        account.migrationResult = MigrationResult.Failure
        assertEquals(
            MigrationResult.Failure,
            manager.migrateFromAccount(migratableAccount, reuseSessionToken = true)
        )

        assertEquals("session", account.latestMigrateAuthInfo!!.sessionToken)
        assertEquals("kSync", account.latestMigrateAuthInfo!!.kSync)
        assertEquals("kXCS", account.latestMigrateAuthInfo!!.kXCS)

        assertNull(manager.authenticatedAccount())

        // Prepare for a successful migration.
        `when`(constellation.finalizeDevice(eq(AuthType.MigratedReuse), any())).thenReturn(ServiceResult.Ok)

        // Success.
        account.migrationResult = MigrationResult.Success
        migratableAccount = migratableAccount.copy(
            authInfo = MigratingAccountInfo("session2", "kSync2", "kXCS2")
        )

        assertEquals(
            MigrationResult.Success,
            manager.migrateFromAccount(migratableAccount, reuseSessionToken = true)
        )

        assertEquals("session2", account.latestMigrateAuthInfo!!.sessionToken)
        assertEquals("kSync2", account.latestMigrateAuthInfo!!.kSync)
        assertEquals("kXCS2", account.latestMigrateAuthInfo!!.kXCS)

        assertNotNull(manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        verify(constellation, times(1)).finalizeDevice(eq(AuthType.MigratedReuse), any())
        verify(accountObserver, times(1)).onAuthenticated(account, AuthType.MigratedReuse)
    }

    @Test
    fun `migrating an account via migrateAccountAsync - retry scenario`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)
        val accountObserver: AccountObserver = mock()

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }
        manager.register(accountObserver)

        // We don't have an account at the start.
        `when`(accountStorage.read()).thenReturn(null)
        manager.start()

        // Bad package name.
        val migratableAccount = ShareableAccount(
            email = "test@example.com",
            sourcePackage = "org.mozilla.firefox",
            authInfo = MigratingAccountInfo("session", "kSync", "kXCS")
        )

        // TODO Need to mock inputs into - mock a PackageManager, and have it return PackageInfo with the right signature.
//        AccountSharing.isTrustedPackage

        // We failed to migrate for some reason. 'WillRetry' in this case assumes reuse flow.
        account.migrationResult = MigrationResult.WillRetry
        assertEquals(
            MigrationResult.WillRetry,
            manager.migrateFromAccount(migratableAccount, reuseSessionToken = true)
        )

        assertEquals("session", account.latestMigrateAuthInfo!!.sessionToken)
        assertEquals("kSync", account.latestMigrateAuthInfo!!.kSync)
        assertEquals("kXCS", account.latestMigrateAuthInfo!!.kXCS)

        assertNotNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Prepare for a successful migration.
        `when`(constellation.finalizeDevice(eq(AuthType.MigratedReuse), any())).thenReturn(ServiceResult.Ok)
        account.migrationResult = MigrationResult.Success
        account.migrationRetrySuccess = true

        // 'sync now' user action will trigger a sign-in retry.
        manager.syncNow(SyncReason.User)

        assertNotNull(manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        verify(constellation, times(1)).finalizeDevice(eq(AuthType.MigratedReuse), any())
        verify(accountObserver, times(1)).onAuthenticated(account, AuthType.MigratedReuse)
    }

    @Test
    fun `restored account has an in-flight migration, retries and fails`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.finalizeDevice(eq(AuthType.MigratedReuse), any())).thenReturn(ServiceResult.Ok)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        account.migrationResult = MigrationResult.WillRetry
        account.migrationRetrySuccess = false

        assertNull(account.persistenceCallback)
        manager.start()
        // Make sure a persistence callback was registered while pumping the state machine.
        assertNotNull(account.persistenceCallback)

        // Assert that neither ensureCapabilities nor initialization fired.
        verify(constellation, never()).finalizeDevice(any(), any())

        // Assert that we do not refresh device state.
        verify(constellation, never()).refreshDevices()

        // Finally, assert that we see an account with an inflight migration.
        assertNotNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())
    }

    @Test
    fun `restored account has an in-flight migration, retries and succeeds`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.finalizeDevice(eq(AuthType.MigratedReuse), any())).thenReturn(ServiceResult.Ok)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        account.migrationResult = MigrationResult.WillRetry
        account.migrationRetrySuccess = true

        assertNull(account.persistenceCallback)
        manager.start()
        // Make sure a persistence callback was registered while pumping the state machine.
        assertNotNull(account.persistenceCallback)

        verify(constellation).finalizeDevice(eq(AuthType.MigratedReuse), any())

        // Finally, assert that we see an account in good standing.
        assertNotNull(manager.authenticatedAccount())
        assertFalse(manager.accountNeedsReauth())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `restored account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.finalizeDevice(eq(AuthType.Existing), any())).thenReturn(ServiceResult.Ok)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)
        manager.start()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that ensureCapabilities fired, but not the device initialization (since we're restoring).
        verify(constellation).finalizeDevice(eq(AuthType.Existing), any())

        // Assert that we refresh device state.
        verify(constellation).refreshDevices()

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `restored account state persistence, finalizeDevice hit an intermittent error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.finalizeDevice(eq(AuthType.Existing), any())).thenReturn(ServiceResult.OtherError)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)
        manager.start()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that finalizeDevice fired with a correct auth type. 3 times since we re-try.
        verify(constellation, times(3)).finalizeDevice(eq(AuthType.Existing), any())

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")

        // Since we weren't able to finalize the account state, we're no longer authenticated.
        assertNull(manager.authenticatedAccount())
    }

    @Test
    fun `restored account state persistence, hit an auth error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation, ableToRecoverFromAuthError = false)

        val accountObserver: AccountObserver = mock()
        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        manager.register(accountObserver)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.AuthError)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)

        assertFalse(manager.accountNeedsReauth())
        assertFalse(account.authErrorDetectedCalled)
        assertFalse(account.checkAuthorizationStatusCalled)
        verify(accountObserver, never()).onAuthenticationProblems()

        manager.start()

        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        assertTrue(account.authErrorDetectedCalled)
        assertTrue(account.checkAuthorizationStatusCalled)
    }

    @Test(expected = FxaPanicException::class)
    fun `restored account state persistence, hit an fxa panic which is re-thrown`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val accountObserver: AccountObserver = mock()
        val manager = TestableFxaAccountManager(
            testContext, ServerConfig(Server.RELEASE, "dummyId", "http://auth-url/redirect"), accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        manager.register(accountObserver)

        // Hit a panic while we're restoring account.
        doAnswer {
            throw FxaPanicException("don't panic!")
        }.`when`(constellation).finalizeDevice(any(), any())

        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)

        assertFalse(manager.accountNeedsReauth())
        verify(accountObserver, never()).onAuthenticationProblems()

        manager.start()
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
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        manager.register(accountObserver)

        // Kick it off, we'll get into a "NotAuthenticated" state.
        manager.start()

        // Perform authentication.

        assertEquals("auth://url", manager.beginAuthentication())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        // Assert that initDevice fired, but not ensureCapabilities (since we're initing a new account).
        verify(constellation).finalizeDevice(eq(AuthType.Signin), any())

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `auth state verification while finishing authentication`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val constellation: DeviceConstellation = mockDeviceConstellation()
        val account = StatePersistenceTestableAccount(profile, constellation)
        val accountObserver: AccountObserver = mock()
        // We are not using the "prepareHappy..." helper method here, because our account isn't a mock,
        // but an actual implementation of the interface.
        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage,
            setOf(DeviceCapability.SEND_TAB), null, this.coroutineContext
        ) {
            account
        }

        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        manager.register(accountObserver)
        // Kick it off, we'll get into a "NotAuthenticated" state.
        manager.start()

        // Attempt to finish authentication without starting it first.
        assertFalse(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        // Start authentication. StatePersistenceTestableAccount will produce state=EXPECTED_AUTH_STATE.
        assertEquals("auth://url", manager.beginAuthentication())

        // Attempt to finish authentication with a wrong state.
        assertFalse(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", UNEXPECTED_AUTH_STATE)))

        // Now attempt to finish it with a correct state.
        `when`(constellation.finalizeDevice(eq(AuthType.Signin), any())).thenReturn(ServiceResult.Ok)
        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        // Assert that manager is authenticated.
        assertEquals(account, manager.authenticatedAccount())
    }

    class StatePersistenceTestableAccount(
        private val profile: Profile,
        private val constellation: DeviceConstellation,
        val ableToRecoverFromAuthError: Boolean = false,
        val tokenServerEndpointUrl: String? = null,
        var migrationResult: MigrationResult = MigrationResult.Failure,
        var migrationRetrySuccess: Boolean = false,
        val accessToken: (() -> AccessTokenInfo)? = null
    ) : OAuthAccount {

        var persistenceCallback: StatePersistenceCallback? = null
        var checkAuthorizationStatusCalled = false
        var authErrorDetectedCalled = false
        var latestMigrateAuthInfo: MigratingAccountInfo? = null

        override suspend fun beginOAuthFlow(scopes: Set<String>, entryPoint: String): AuthFlowUrl? {
            return AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url")
        }

        override suspend fun beginPairingFlow(pairingUrl: String, scopes: Set<String>, entryPoint: String): AuthFlowUrl? {
            return AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url")
        }

        override suspend fun getProfile(ignoreCache: Boolean): Profile? {
            return profile
        }

        override fun getCurrentDeviceId(): String? {
            return "testFxaDeviceId"
        }

        override fun getSessionToken(): String? {
            return null
        }

        override suspend fun completeOAuthFlow(code: String, state: String): Boolean {
            return true
        }

        override suspend fun migrateFromAccount(authInfo: MigratingAccountInfo, reuseSessionToken: Boolean): JSONObject? {
            latestMigrateAuthInfo = authInfo
            return when (migrationResult) {
                MigrationResult.Failure, MigrationResult.WillRetry -> null
                MigrationResult.Success -> JSONObject()
            }
        }

        override fun isInMigrationState(): InFlightMigrationState? {
            return when (migrationResult) {
                MigrationResult.Success -> InFlightMigrationState.REUSE_SESSION_TOKEN
                MigrationResult.WillRetry -> InFlightMigrationState.REUSE_SESSION_TOKEN
                else -> null
            }
        }

        override suspend fun retryMigrateFromSessionToken(): JSONObject? {
            return when (migrationRetrySuccess) {
                true -> JSONObject()
                false -> null
            }
        }

        override suspend fun getAccessToken(singleScope: String): AccessTokenInfo? {
            val token = accessToken?.invoke()
            if (token != null) return token

            fail()
            return null
        }

        override fun authErrorDetected() {
            authErrorDetectedCalled = true
        }

        override suspend fun checkAuthorizationStatus(singleScope: String): Boolean? {
            checkAuthorizationStatusCalled = true
            return ableToRecoverFromAuthError
        }

        override suspend fun getTokenServerEndpointURL(): String? {
            if (tokenServerEndpointUrl != null) return tokenServerEndpointUrl

            fail()
            return ""
        }

        override fun getPairingAuthorityURL(): String {
            return "https://firefox.com/pair"
        }

        override fun registerPersistenceCallback(callback: StatePersistenceCallback) {
            persistenceCallback = callback
        }

        override fun deviceConstellation(): DeviceConstellation {
            return constellation
        }

        override suspend fun disconnect(): Boolean {
            return true
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
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        )

        val accountObserver = object : AccountObserver {
            override fun onLoggedOut() {
                fail()
            }

            override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
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
        manager.start()
    }

    @Test
    fun `no persisted account`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        verify(accountObserver, never()).onAuthenticated(any(), any())
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
            "testUid", "test@example.com", null, "Test Profile"
        )
        `when`(mockAccount.getProfile(ignoreCache = false)).thenReturn(profile)
        `when`(mockAccount.isInMigrationState()).thenReturn(null)
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(mockAccount)
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(eq(AuthType.Existing), any())).thenReturn(ServiceResult.Ok)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage,
            emptySet(), null, this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)

        manager.start()

        // Make sure that account and profile observers are fired exactly once.
        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Existing)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, never()).clear()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        // Assert that we don't refresh device state for non-SEND_TAB enabled devices.
        verify(constellation, never()).refreshDevices()

        // Make sure 'logoutAsync' clears out state and fires correct observers.
        reset(accountObserver)
        reset(accountStorage)
        `when`(mockAccount.disconnect()).thenReturn(true)

        // Simulate SyncManager populating SyncEnginesStorage with some state.
        SyncEnginesStorage(testContext).setStatus(SyncEngine.History, true)
        SyncEnginesStorage(testContext).setStatus(SyncEngine.Passwords, false)
        assertTrue(SyncEnginesStorage(testContext).getStatus().isNotEmpty())

        verify(mockAccount, never()).disconnect()
        manager.logout()

        assertTrue(SyncEnginesStorage(testContext).getStatus().isEmpty())
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, times(1)).onLoggedOut()
        verify(mockAccount, times(1)).disconnect()

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
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        val cachedAuthInfo = SyncAuthInfoCache(testContext).getCached()
        assertNotNull(cachedAuthInfo)
        assertEquals("kid", cachedAuthInfo!!.kid)
        assertEquals("someToken", cachedAuthInfo.fxaAccessToken)
        assertEquals("k", cachedAuthInfo.syncKey)
        assertEquals("some://url", cachedAuthInfo.tokenServerUrl)
        assertTrue(cachedAuthInfo.fxaAccessTokenExpiresAt > 0)
    }

    @Test(expected = FxaPanicException::class)
    fun `fxa panic during initDevice flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        doAnswer {
            throw FxaPanicException("Don't panic!")
        }.`when`(constellation).finalizeDevice(any(), any())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))
    }

    @Test(expected = FxaPanicException::class)
    fun `fxa panic during pairing flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(mock())
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        `when`(mockAccount.getProfile(ignoreCache = false)).thenReturn(profile)

        doAnswer {
            throw FxaPanicException("Don't panic!")
        }.`when`(mockAccount).beginPairingFlow(any(), any(), anyString())
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = coroutineContext
        ) {
            mockAccount
        }

        manager.start()
        manager.beginAuthentication("http://pairing.com")
        fail()
    }

    @Test
    fun `happy pairing authentication and profile flow`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication(pairingUrl = "auth://pairing"))
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `repeated unfinished authentication attempts succeed`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        // Begin auth for the first time.
        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication(pairingUrl = "auth://pairing"))
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Now, try to begin again before finishing the first one.
        assertEquals("auth://url", manager.beginAuthentication(pairingUrl = "auth://pairing"))
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // The rest should "just work".
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
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
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)

        assertNull(manager.beginAuthentication())

        // Confirm that account state observable doesn't receive authentication errors.
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertEquals("auth://url", manager.beginAuthentication())

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())
        verify(accountStorage, times(1)).clear()

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, times(1)).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
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

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)

        assertNull(manager.beginAuthentication(pairingUrl = "auth://pairing"))

        // Confirm that account state observable doesn't receive authentication errors.
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginPairingFlow(anyString(), any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertEquals("auth://url", manager.beginAuthentication(pairingUrl = "auth://pairing"))

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())
        verify(accountStorage, times(1)).clear()

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, times(1)).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `authentication issues are propagated via AccountObserver`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, this.coroutineContext)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        // Our recovery flow should attempt to hit this API. Model the "can't recover" condition by returning 'false'.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(false)

        // At this point, we're logged in. Trigger a 401.
        manager.encounteredAuthError("a test")

        verify(accountObserver, times(1)).onAuthenticationProblems()
        assertTrue(manager.accountNeedsReauth())
        assertEquals(mockAccount, manager.authenticatedAccount())

        // Make sure profile is still available.
        assertEquals(profile, manager.accountProfile())

        // Able to re-authenticate.
        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Pairing, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountObserver).onAuthenticated(mockAccount, AuthType.Pairing)
        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())
        assertEquals(profile, manager.accountProfile())
    }

    @Test
    fun `authentication issues are recoverable via checkAuthorizationState`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val crashReporter: CrashReporting = mock()
        val manager = prepareHappyAuthenticationFlow(
            mockAccount,
            profile,
            accountStorage,
            accountObserver,
            this.coroutineContext,
            setOf(DeviceCapability.SEND_TAB),
            crashReporter
        )

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)
        `when`(constellation.refreshDevices()).thenReturn(true)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        // Recovery flow will hit this API, and will recover if it returns 'true'.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(true)

        // At this point, we're logged in. Trigger a 401.
        manager.encounteredAuthError("a test")
        assertRecovered(true, "a test", constellation, accountObserver, manager, mockAccount, crashReporter)
    }

    @Test
    fun `authentication recovery flow has a circuit breaker`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val crashReporter: CrashReporting = mock()
        val manager = prepareHappyAuthenticationFlow(
            mockAccount,
            profile,
            accountStorage,
            accountObserver,
            this.coroutineContext,
            setOf(DeviceCapability.SEND_TAB),
            crashReporter
        )
        GlobalAccountManager.setInstance(manager)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)
        `when`(constellation.refreshDevices()).thenReturn(true)

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        // Recovery flow will hit this API, and will recover if it returns 'true'.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(true)

        // At this point, we're logged in. Trigger a 401 for the first time.
        manager.encounteredAuthError("a test")
        // ... and just for good measure, trigger another 401 to simulate overlapping API calls legitimately hitting 401s.
        manager.encounteredAuthError("a test", errorCountWithinTheTimeWindow = 3)
        assertRecovered(true, "a test", constellation, accountObserver, manager, mockAccount, crashReporter)

        // We've fully recovered by now, let's hit another 401 sometime later (count has been reset).
        manager.encounteredAuthError("a test")
        assertRecovered(true, "a test", constellation, accountObserver, manager, mockAccount, crashReporter)

        // Suddenly, we're in a bad loop, expect to hit our circuit-breaker here.
        manager.encounteredAuthError("another test", errorCountWithinTheTimeWindow = 50)
        assertRecovered(false, "another test", constellation, accountObserver, manager, mockAccount, crashReporter)
    }

    private suspend fun assertRecovered(
        success: Boolean,
        operation: String,
        constellation: DeviceConstellation,
        accountObserver: AccountObserver,
        manager: FxaAccountManager,
        mockAccount: OAuthAccount,
        crashReporter: CrashReporting
    ) {
        // During recovery, only 'sign-in' finalize device call should have been made.
        verify(constellation, times(1)).finalizeDevice(eq(AuthType.Signin), any())
        verify(constellation, never()).finalizeDevice(eq(AuthType.Recovered), any())
        verify(constellation, times(1)).refreshDevices()

        assertEquals(mockAccount, manager.authenticatedAccount())

        if (success) {
            // Since we've recovered, outside observers should not have witnessed the momentary problem state.
            verify(accountObserver, never()).onAuthenticationProblems()
            assertFalse(manager.accountNeedsReauth())
            verifyNoInteractions(crashReporter)
        } else {
            // We were unable to recover, outside observers should have been told.
            verify(accountObserver, times(1)).onAuthenticationProblems()
            assertTrue(manager.accountNeedsReauth())

            val captor = argumentCaptor<Throwable>()
            verify(crashReporter).submitCaughtException(captor.capture())
            assertEquals("Auth recovery circuit breaker triggered by: $operation", captor.value.message)
            assertTrue(captor.value is AccountManagerException.AuthRecoveryCircuitBreakerException)
        }
    }

    @Test
    fun `unhappy profile fetching flow`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)
        `when`(mockAccount.getProfile(ignoreCache = false)).thenReturn(null)
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount, AuthType.Signin)
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Make sure we can re-try fetching a profile. This time, let's have it succeed.
        reset(accountObserver)
        val profile = Profile(
            uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile"
        )

        `when`(mockAccount.getProfile(ignoreCache = true)).thenReturn(profile)
        assertNull(manager.accountProfile())
        assertEquals(profile, manager.fetchProfile())

        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onLoggedOut()
    }

    @Test
    fun `profile fetching flow hit an unrecoverable auth problem`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        // Our recovery flow should attempt to hit this API. Model the "can't recover" condition by returning false.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(false)

        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        lateinit var waitFor: Job
        `when`(mockAccount.getProfile(ignoreCache = false)).then {
            // Hit an auth error.
            waitFor = CoroutineScope(coroutineContext).launch { manager.encounteredAuthError("a test") }
            null
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatus(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE))

        waitFor.join()
        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        verify(mockAccount, times(1)).checkAuthorizationStatus(eq("profile"))
        Unit
    }

    @Test
    fun `profile fetching flow hit an unrecoverable auth problem for which we can't determine a recovery state`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        // Our recovery flow should attempt to hit this API. Model the "don't know what's up" condition by returning null.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(null)

        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        lateinit var waitFor: Job
        `when`(mockAccount.getProfile(ignoreCache = false)).then {
            // Hit an auth error.
            waitFor = CoroutineScope(coroutineContext).launch { manager.encounteredAuthError("a test") }
            null
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatus(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))

        waitFor.join()
        assertTrue(manager.accountNeedsReauth())
        verify(accountObserver, times(1)).onAuthenticationProblems()
        verify(mockAccount, times(1)).checkAuthorizationStatus(eq("profile"))
        Unit
    }

    @Test
    fun `profile fetching flow hit a recoverable auth problem`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val captor = argumentCaptor<AuthType>()

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)

        val profile = Profile(
            uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile"
        )

        // Recovery flow will hit this API, return a success.
        `when`(mockAccount.checkAuthorizationStatus(eq("profile"))).thenReturn(true)

        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        var didFailProfileFetch = false
        lateinit var waitFor: Job
        `when`(mockAccount.getProfile(ignoreCache = false)).then {
            // Hit an auth error, but only once. As we recover from it, we'll attempt to fetch a profile
            // again. At that point, we'd like to succeed.
            if (!didFailProfileFetch) {
                didFailProfileFetch = true
                waitFor = CoroutineScope(coroutineContext).launch { manager.encounteredAuthError("a test") }
                null
            } else {
                profile
            }
        }
        // Upon recovery, we'll hit an 'ignore cache' path.
        `when`(mockAccount.getProfile(ignoreCache = true)).thenReturn(profile)

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onAuthenticationProblems()
        verify(mockAccount, never()).checkAuthorizationStatus(any())
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signup, "dummyCode", EXPECTED_AUTH_STATE)))
        waitFor.join()
        assertFalse(manager.accountNeedsReauth())
        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())
        verify(accountObserver, never()).onAuthenticationProblems()
        // Once for the initial auth success, then once again after we recover from an auth problem.
        verify(accountObserver, times(2)).onAuthenticated(eq(mockAccount), captor.capture())
        assertEquals(AuthType.Signup, captor.allValues[0])
        assertEquals(AuthType.Recovered, captor.allValues[1])
        // Verify that we went through the recovery flow.
        verify(mockAccount, times(1)).checkAuthorizationStatus(eq("profile"))
        Unit
    }

    @Test(expected = FxaPanicException::class)
    fun `profile fetching flow hit an fxa panic, which is re-thrown`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        `when`(mockAccount.getCurrentDeviceId()).thenReturn("testDeviceId")
        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.finalizeDevice(any(), any())).thenReturn(ServiceResult.Ok)
        doAnswer {
            throw FxaPanicException("500")
        }.`when`(mockAccount).getProfile(ignoreCache = false)
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.start()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any(), any())
        verify(accountObserver, never()).onAuthenticationProblems()
        assertFalse(manager.accountNeedsReauth())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthentication())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        assertTrue(manager.finishAuthentication(FxaAuthData(AuthType.Signin, "dummyCode", EXPECTED_AUTH_STATE)))
    }

    @Test
    fun `accounts to sync integration`() {
        val syncManager: SyncManager = mock()
        val integration = FxaAccountManager.AccountsToSyncIntegration(syncManager)

        // onAuthenticated - mapping of AuthType to SyncReason
        integration.onAuthenticated(mock(), AuthType.Signin)
        verify(syncManager, times(1)).start()
        verify(syncManager, times(1)).now(eq(SyncReason.FirstSync), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.Signup)
        verify(syncManager, times(2)).start()
        verify(syncManager, times(2)).now(eq(SyncReason.FirstSync), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.Pairing)
        verify(syncManager, times(3)).start()
        verify(syncManager, times(3)).now(eq(SyncReason.FirstSync), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.MigratedReuse)
        verify(syncManager, times(4)).start()
        verify(syncManager, times(4)).now(eq(SyncReason.FirstSync), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.OtherExternal("test"))
        verify(syncManager, times(5)).start()
        verify(syncManager, times(5)).now(eq(SyncReason.FirstSync), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.Existing)
        verify(syncManager, times(6)).start()
        verify(syncManager, times(1)).now(eq(SyncReason.Startup), anyBoolean())
        integration.onAuthenticated(mock(), AuthType.Recovered)
        verify(syncManager, times(7)).start()
        verify(syncManager, times(2)).now(eq(SyncReason.Startup), anyBoolean())
        verifyNoMoreInteractions(syncManager)

        // onProfileUpdated - no-op
        integration.onProfileUpdated(mock())
        verifyNoMoreInteractions(syncManager)

        // onAuthenticationProblems
        integration.onAuthenticationProblems()
        verify(syncManager).stop()
        verifyNoMoreInteractions(syncManager)

        // onLoggedOut
        integration.onLoggedOut()
        verify(syncManager, times(2)).stop()
        verifyNoMoreInteractions(syncManager)
    }

    private suspend fun prepareHappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver,
        coroutineContext: CoroutineContext,
        capabilities: Set<DeviceCapability> = emptySet(),
        crashReporter: CrashReporting? = null
    ): FxaAccountManager {

        val accessTokenInfo = AccessTokenInfo(
            "testSc0pe", "someToken", OAuthScopedKey("kty", "testSc0pe", "kid", "k"), System.currentTimeMillis() + 1000 * 10
        )

        `when`(mockAccount.getProfile(ignoreCache = false)).thenReturn(profile)
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.beginPairingFlow(anyString(), any(), anyString())).thenReturn(AuthFlowUrl(EXPECTED_AUTH_STATE, "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        `when`(mockAccount.getAccessToken(anyString())).thenReturn(accessTokenInfo)
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("some://url")

        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, capabilities,
            SyncConfig(setOf(SyncEngine.History, SyncEngine.Bookmarks), PeriodicSyncConfig()),
            coroutineContext = coroutineContext, crashReporter = crashReporter
        ) {
            mockAccount
        }

        manager.register(accountObserver)
        manager.start()

        return manager
    }

    private suspend fun prepareUnhappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver,
        coroutineContext: CoroutineContext
    ): FxaAccountManager {
        `when`(mockAccount.getProfile(ignoreCache = false)).thenReturn(profile)
        `when`(mockAccount.deviceConstellation()).thenReturn(mock())
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(null)
        `when`(mockAccount.beginPairingFlow(anyString(), any(), anyString())).thenReturn(null)
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            accountStorage, coroutineContext = coroutineContext
        ) {
            mockAccount
        }

        manager.register(accountObserver)

        runBlocking(coroutineContext) {
            manager.start()
        }

        return manager
    }

    private suspend fun mockDeviceConstellation(): DeviceConstellation {
        val c: DeviceConstellation = mock()
        `when`(c.refreshDevices()).thenReturn(true)
        return c
    }
}
