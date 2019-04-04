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
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
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

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: Config,
    scopes: Array<String>,
    private val storage: AccountStorage,
    val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, scopes, null) {
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
    }

    @Test
    fun `restored account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile("testUid", "test@example.com", null, "Test Profile")
        val account = object : OAuthAccount {
            var persistenceCallback: StatePersistenceCallback? = null

            override fun beginOAuthFlow(scopes: Array<String>, wantsKeys: Boolean): Deferred<String> {
                fail()
                return CompletableDeferred()
            }

            override fun beginPairingFlow(pairingUrl: String, scopes: Array<String>): Deferred<String> {
                fail()
                return CompletableDeferred()
            }

            override fun getProfile(ignoreCache: Boolean): Deferred<Profile> {
                return CompletableDeferred(profile)
            }

            override fun getProfile(): Deferred<Profile> {
                return CompletableDeferred(profile)
            }

            override fun completeOAuthFlow(code: String, state: String): Deferred<Unit> {
                fail()
                return CompletableDeferred()
            }

            override fun getAccessToken(singleScope: String): Deferred<AccessTokenInfo> {
                fail()
                return CompletableDeferred()
            }

            override fun getTokenServerEndpointURL(): String {
                fail()
                return ""
            }

            override fun registerPersistenceCallback(callback: StatePersistenceCallback) {
                persistenceCallback = callback
            }

            override fun toJSONString(): String {
                fail()
                return ""
            }

            override fun close() {
                fail()
            }
        }

        val manager = TestableFxaAccountManager(
            context, Config.release("dummyId", "http://auth-url/redirect"), arrayOf("profile"), accountStorage
        ) {
            account
        }

        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(account)

        assertNull(account.persistenceCallback)
        manager.initAsync().await()

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    @Test
    fun `newly authenticated account state persistence`() = runBlocking {
        val accountStorage: AccountStorage = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val account = StatePersistenceTestableAccount(profile)
        val accountObserver: AccountObserver = mock()
        // We are not using the "prepareHappy..." helper method here, because our account isn't a mock,
        // but an actual implementation of the interface.
        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage
        ) {
            account
        }

        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        manager.register(accountObserver)

        // Kick it off, we'll get into a "NotAuthenticated" state.
        runBlocking {
            manager.initAsync().await()
        }

        assertNull(account.persistenceCallback)

        // Perform authentication.
        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        }
        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

        // Assert that persistence callback is set.
        assertNotNull(account.persistenceCallback)

        // Assert that persistence callback is interacting with the storage layer.
        account.persistenceCallback!!.persist("test")
        verify(accountStorage).write("test")
    }

    class StatePersistenceTestableAccount(private val profile: Profile) : OAuthAccount {
        var persistenceCallback: StatePersistenceCallback? = null

        override fun beginOAuthFlow(scopes: Array<String>, wantsKeys: Boolean): Deferred<String> {
            return CompletableDeferred("auth://url")
        }

        override fun beginPairingFlow(pairingUrl: String, scopes: Array<String>): Deferred<String> {
            return CompletableDeferred("auth://url")
        }

        override fun getProfile(ignoreCache: Boolean): Deferred<Profile> {
            return CompletableDeferred(profile)
        }

        override fun getProfile(): Deferred<Profile> {
            return CompletableDeferred(profile)
        }

        override fun completeOAuthFlow(code: String, state: String): Deferred<Unit> {
            // This ceremony is necessary because CompletableDeferred<Unit>() is created in an _active_ state,
            // and threads will deadlock since it'll never be resolved while state machine is waiting for it.
            // So we manually complete it here!
            val unitDeferred = CompletableDeferred<Unit>()
            unitDeferred.complete(Unit)
            return unitDeferred
        }

        override fun getAccessToken(singleScope: String): Deferred<AccessTokenInfo> {
            fail()
            return CompletableDeferred()
        }

        override fun getTokenServerEndpointURL(): String {
            fail()
            return ""
        }

        override fun registerPersistenceCallback(callback: StatePersistenceCallback) {
            persistenceCallback = callback
        }

        override fun toJSONString(): String {
            fail()
            return ""
        }

        override fun close() {
            fail()
        }
    }

    @Test
    fun `error reading persisted account`() {
        val accountStorage = mock<AccountStorage>()
        val readException = FxaException("pretend we failed to parse the account")
        `when`(accountStorage.read()).thenThrow(readException)

        val manager = TestableFxaAccountManager(
            context,
            Config.release("dummyId", "bad://url"),
            arrayOf("profile"),
            accountStorage
        )

        var onErrorCalled = false

        val accountObserver = object : AccountObserver {
            override fun onLoggedOut() {
                fail()
            }

            override fun onAuthenticated(account: OAuthAccount) {
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

        runBlocking {
            manager.initAsync().await()
        }

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
                accountStorage
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
        val profile = Profile(
                "testUid", "test@example.com", null, "Test Profile")
        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(CompletableDeferred(profile))
        // We have an account at the start.
        `when`(accountStorage.read()).thenReturn(mockAccount)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile"),
                accountStorage
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
        manager.logoutAsync().await()

        verify(accountObserver, never()).onError(any())
        verify(accountObserver, never()).onAuthenticated(any())
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, times(1)).onLoggedOut()

        verify(accountStorage, never()).read()
        verify(accountStorage, never()).write(any())
        verify(accountStorage, times(1)).clear()

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        manager.close()

        verify(mockAccount, times(1)).close()
    }

    @Test
    fun `happy authentication and profile flow`() {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        }
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

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
    fun `happy pairing authentication and profile flow`() {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountStorage = mock<AccountStorage>()
        val accountObserver: AccountObserver = mock()
        val manager = prepareHappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())
        }
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

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
    fun `unhappy authentication flow`() {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountObserver: AccountObserver = mock()
        val fxaException = FxaNetworkException("network problem")
        val manager = prepareUnhappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, fxaException)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        runBlocking {
            try {
                manager.beginAuthenticationAsync().await()
                fail()
            } catch (e: FxaNetworkException) {
                assertEquals(fxaException.message, e.message)
            }
        }
        // Confirm that account state observable doesn't receive authentication errors.
        verify(accountObserver, never()).onError(any())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginOAuthFlow(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))

        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        }

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

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
    fun `unhappy pairing authentication flow`() {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")
        val accountObserver: AccountObserver = mock()
        val fxaException = FxaNetworkException("network problem")
        val manager = prepareUnhappyAuthenticationFlow(mockAccount, profile, accountStorage, accountObserver, fxaException)

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        runBlocking {
            try {
                manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await()
                fail()
            } catch (e: FxaNetworkException) {
                assertEquals(fxaException.message, e.message)
            }
        }
        // Confirm that account state observable doesn't receive authentication errors.
        verify(accountObserver, never()).onError(any())
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Try again, without any network problems this time.
        `when`(mockAccount.beginPairingFlow(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))

        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync(pairingUrl = "auth://pairing").await())
        }

        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

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
    fun `unhappy profile fetching flow`() {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()

        val exceptionalProfile = CompletableDeferred<Profile>()
        val fxaException = FxaException("test exception")
        exceptionalProfile.completeExceptionally(fxaException)

        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(exceptionalProfile)
        `when`(mockAccount.beginOAuthFlow(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        // This ceremony is necessary because CompletableDeferred<Unit>() is created in an _active_ state,
        // and threads will deadlock since it'll never be resolved while state machine is waiting for it.
        // So we manually complete it here!
        val unitDeferred = CompletableDeferred<Unit>()
        unitDeferred.complete(Unit)
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(unitDeferred)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage
        ) {
            mockAccount
        }

        val accountObserver: AccountObserver = mock()

        manager.register(accountObserver)

        runBlocking {
            manager.initAsync().await()
        }

        // We start off as logged-out, but the event won't be called (initial default state is assumed).
        verify(accountObserver, never()).onLoggedOut()
        verify(accountObserver, never()).onAuthenticated(any())

        reset(accountObserver)
        runBlocking {
            assertEquals("auth://url", manager.beginAuthenticationAsync().await())
        }
        assertNull(manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        runBlocking {
            manager.finishAuthenticationAsync("dummyCode", "dummyState").await()
        }

        verify(accountStorage, times(1)).read()
        verify(accountStorage, never()).clear()

        val captor = argumentCaptor<FxaException>()
        verify(accountObserver, times(1)).onError(captor.capture())
        assertEquals(fxaException.message, captor.value.message)

        verify(accountObserver, times(1)).onAuthenticated(mockAccount)
        verify(accountObserver, never()).onProfileUpdated(any())
        verify(accountObserver, never()).onLoggedOut()

        assertEquals(mockAccount, manager.authenticatedAccount())
        assertNull(manager.accountProfile())

        // Make sure we can re-try fetching a profile. This time, let's have it succeed.
        reset(accountObserver)
        val profile = Profile(
            uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(CompletableDeferred(profile))

        runBlocking {
            manager.updateProfileAsync().await()
        }

        verify(accountObserver, times(1)).onProfileUpdated(profile)
        verify(accountObserver, never()).onError(any())
        verify(accountObserver, never()).onAuthenticated(any())
        verify(accountObserver, never()).onLoggedOut()
        assertEquals(profile, manager.accountProfile())
    }

    private fun prepareHappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver
    ): FxaAccountManager {

        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(CompletableDeferred(profile))
        `when`(mockAccount.beginOAuthFlow(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.beginPairingFlow(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        // This ceremony is necessary because CompletableDeferred<Unit>() is created in an _active_ state,
        // and threads will deadlock since it'll never be resolved while state machine is waiting for it.
        // So we manually complete it here!
        val unitDeferred = CompletableDeferred<Unit>()
        unitDeferred.complete(Unit)
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(unitDeferred)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage
        ) {
            mockAccount
        }

        manager.register(accountObserver)

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }

    private fun prepareUnhappyAuthenticationFlow(
        mockAccount: OAuthAccount,
        profile: Profile,
        accountStorage: AccountStorage,
        accountObserver: AccountObserver,
        fxaException: FxaException
    ): FxaAccountManager {
        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(CompletableDeferred(profile))

        // Pretend we have a network problem while initiating an auth flow.
        val exceptionalDeferred = CompletableDeferred<String>()
        exceptionalDeferred.completeExceptionally(fxaException)
        `when`(mockAccount.beginOAuthFlow(any(), anyBoolean())).thenReturn(exceptionalDeferred)
        `when`(mockAccount.beginPairingFlow(anyString(), any())).thenReturn(exceptionalDeferred)

        // This ceremony is necessary because CompletableDeferred<Unit>() is created in an _active_ state,
        // and threads will deadlock since it'll never be resolved while state machine is waiting for it.
        // So we manually complete it here!
        val unitDeferred = CompletableDeferred<Unit>()
        unitDeferred.complete(Unit)
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(unitDeferred)
        // There's no account at the start.
        `when`(accountStorage.read()).thenReturn(null)

        val manager = TestableFxaAccountManager(
                context,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage
        ) {
            mockAccount
        }

        manager.register(accountObserver)

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }
}
