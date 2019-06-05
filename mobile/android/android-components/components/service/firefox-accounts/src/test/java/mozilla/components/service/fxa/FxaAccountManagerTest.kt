/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import androidx.test.core.app.ApplicationProvider
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
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.DeviceTuple
import mozilla.components.service.fxa.manager.Event
import mozilla.components.service.fxa.manager.FailedToLoadAccountException
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.authErrorRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
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
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import kotlin.coroutines.CoroutineContext

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: Config,
    scopes: Array<String>,
    private val storage: AccountStorage,
    capabilities: List<DeviceCapability> = listOf(),
    coroutineContext: CoroutineContext,
    val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, scopes, DeviceTuple("test", DeviceType.UNKNOWN, capabilities), null, coroutineContext) {
    override fun createAccount(config: Config): OAuthAccount {
        return block()
    }

    override fun getAccountStorage(): AccountStorage {
        return storage
    }
}

@RunWith(RobolectricTestRunner::class)
class FxaAccountManagerTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @After
    fun cleanup() {
        // This registry is global, so we need to clear it between test runs, otherwise different
        // manager instances will be kept around.
        authErrorRegistry.unregisterObservers()
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
    }

    @Test
    fun `restored account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
            context, Config.release("dummyId", "http://auth-url/redirect"), arrayOf("profile"), accountStorage,
            listOf(DeviceCapability.SEND_TAB), this.coroutineContext
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
        verify(constellation).ensureCapabilitiesAsync(listOf(DeviceCapability.SEND_TAB))
        verify(constellation, never()).initDeviceAsync(any(), any(), any())

        // Assert that periodic account refresh started.
        verify(constellation).startPeriodicRefresh()

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `restored account state persistence, ensureCapabilities hit an intermittent error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation)

        val manager = TestableFxaAccountManager(
                context, Config.release("dummyId", "http://auth-url/redirect"), arrayOf("profile"), accountStorage,
                listOf(DeviceCapability.SEND_TAB), this.coroutineContext
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
        verify(constellation).ensureCapabilitiesAsync(listOf(DeviceCapability.SEND_TAB))
        verify(constellation, never()).initDeviceAsync(any(), any(), any())

        // Assert that periodic account refresh started.
        verify(constellation).startPeriodicRefresh()

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `restored account state persistence, hit an auth error`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation, ableToRecoverFromAuthError = false)

        val accountObserver: AccountObserver = mock()
        val manager = TestableFxaAccountManager(
                context, Config.release("dummyId", "http://auth-url/redirect"), arrayOf("profile"), accountStorage,
                listOf(DeviceCapability.SEND_TAB), this.coroutineContext
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
                context, Config.release("dummyId", "http://auth-url/redirect"), arrayOf("profile"), accountStorage,
                listOf(DeviceCapability.SEND_TAB), this.coroutineContext
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
        val constellation: DeviceConstellation = mock()
        val account = StatePersistenceTestableAccount(profile, constellation)
        val accountObserver: AccountObserver = mock()
        // We are not using the "prepareHappy..." helper method here, because our account isn't a mock,
        // but an actual implementation of the interface.
        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage,
                listOf(DeviceCapability.SEND_TAB), this.coroutineContext
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

        // Assert that periodic account refresh started after finishing auth.
        verify(constellation).startPeriodicRefresh()

        // Assert that initDevice fired, but not ensureCapabilities (since we're initing a new account).
        verify(constellation).initDeviceAsync(any(), any(), eq(listOf(DeviceCapability.SEND_TAB)))
        verify(constellation, never()).ensureCapabilitiesAsync(any())

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    class StatePersistenceTestableAccount(private val profile: Profile, private val constellation: DeviceConstellation, val ableToRecoverFromAuthError: Boolean = false) : OAuthAccount {
        var persistenceCallback: StatePersistenceCallback? = null
        var accessTokenErrorCalled = false

        override fun beginOAuthFlowAsync(scopes: Set<String>, wantsKeys: Boolean): Deferred<String?> {
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

        override fun getAccessTokenAsync(
            singleScope: String
        ): Deferred<AccessTokenInfo?> {
            fail()
            return CompletableDeferred(null)
        }

        override fun checkAuthorizationStatusAsync(singleScope: String): Deferred<Boolean?> {
            accessTokenErrorCalled = true
            return CompletableDeferred(ableToRecoverFromAuthError)
        }

        override fun getTokenServerEndpointURL(): String {
            fail()
            return ""
        }

        override fun registerPersistenceCallback(callback: StatePersistenceCallback) {
            persistenceCallback = callback
        }

        override fun deviceConstellation(): DeviceConstellation {
            return constellation
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
            context,
            Config.release("dummyId", "bad://url"),
            arrayOf("profile"),
            accountStorage, coroutineContext = this.coroutineContext
        )

        var onErrorCalled = false

        val accountObserver = object : AccountObserver {
            override fun onLoggedOut() {
                fail()
            }

            override fun onAuthenticated(account: OAuthAccount) {
                fail()
            }

            override fun onAuthenticationProblems() {
                fail()
            }

            override fun onProfileUpdated(profile: Profile) {
                fail()
            }

            override fun onError(error: Exception) {
                assertFalse(onErrorCalled)
                onErrorCalled = true
                assertTrue(error is FailedToLoadAccountException)
                assertEquals(error.cause, readException)
            }
        }

        manager.register(accountObserver)

        manager.initAsync().await()

        assertTrue(onErrorCalled)
    }

    @Test
    fun `no persisted account`() = runBlocking {
        val accountStorage = mock<AccountStorage>()
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile"),
                accountStorage, coroutineContext = this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, never()).onAuthenticated(any())
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
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile"),
                accountStorage,
                emptyList(), this.coroutineContext
        )

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)

        manager.initAsync().await()

        // Make sure that account and profile observers are fired exactly once.
        verify(accountObserver, never()).onError(any())
        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onLoggedOut()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, never()).clear()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertEquals(profile, manager.accountProfile())

        // Make sure 'logoutAsync' clears out state and fires correct observers.
        reset(accountObserver)
        reset(accountStorage)
        `when`(constellation.destroyCurrentDeviceAsync()).thenReturn(CompletableDeferred(true))
        verify(constellation, never()).destroyCurrentDeviceAsync()
        manager.logoutAsync().await()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, never()).onAuthenticated(any())
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, times(1)).onLoggedOut()
        verify(constellation, times(1)).destroyCurrentDeviceAsync()

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
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onAuthenticated(any())

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
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
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
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        `when`(mockAccount.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)

        assertNull(manager.beginAuthenticationAsync().await())

        // Confirm that account state observable doesn't receive authentication errors.
        verify(accountObserver, never()).onError(any())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(constellation.initDeviceAsync(any(), any(), any())).thenReturn(CompletableDeferred(true))

        assertEquals("auth://url", manager.beginAuthenticationAsync().await())

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)

        assertNull(manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())

        // Confirm that account state observable doesn't receive authentication errors.
        verify(accountObserver, never()).onError(any())
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

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onAuthenticated(any())

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

        verify(accountObserver).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onAuthenticated(any())

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
        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.finishAuthenticationAsync("dummyCode", "dummyState").await()

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
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
        verify(accountObserver, never()).onError(any())
        verify(accountObserver, never()).onAuthenticated(any())
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

        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())
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

        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())
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
                CompletableDeferred(value = null)
            } else {
                CompletableDeferred(profile)
            }
        }

        // Recovery flow will hit this API, return a success.
        `when`(mockAccount.checkAuthorizationStatusAsync(eq("profile"))).thenReturn(CompletableDeferred(true))

        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())
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
        verify(accountObserver, times(2)).onAuthenticated(mockAccount)
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
        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage, coroutineContext = this.coroutineContext
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)
        manager.initAsync().await()

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())
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
        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
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

        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
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
}