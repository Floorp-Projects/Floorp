/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.store

import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import mozilla.components.concept.sync.AuthFlowError
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.Avatar
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SCOPE_PROFILE
import mozilla.components.support.test.any
import mozilla.components.support.test.coMock
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.lang.Exception

@OptIn(ExperimentalCoroutinesApi::class)
class SyncStoreSupportTest {

    private val accountManager = mock<FxaAccountManager>()
    private val lifecycleOwner = mock<LifecycleOwner>()
    private val autoPause = false
    private val coroutineScope = TestScope()

    private lateinit var store: SyncStore
    private lateinit var syncObserver: AccountSyncObserver
    private lateinit var constellationObserver: ConstellationObserver
    private lateinit var accountObserver: FxaAccountObserver
    private lateinit var integration: SyncStoreSupport

    @Before
    fun setup() {
        Dispatchers.setMain(StandardTestDispatcher(coroutineScope.testScheduler))

        store = SyncStore()
        syncObserver = AccountSyncObserver(store)
        constellationObserver = ConstellationObserver(store)
        accountObserver = FxaAccountObserver(
            store = store,
            deviceConstellationObserver = constellationObserver,
            lifecycleOwner = lifecycleOwner,
            autoPause = autoPause,
            coroutineScope = coroutineScope,
        )

        integration = SyncStoreSupport(
            store = store,
            fxaAccountManager = lazyOf(accountManager),
            lifecycleOwner = lifecycleOwner,
            autoPause = autoPause,
            coroutineScope = coroutineScope,
        )
    }

    @Test
    fun `GIVEN integration WHEN initialize is called THEN observers registered`() {
        integration.initialize()

        verify(accountManager).registerForSyncEvents(any(), eq(lifecycleOwner), eq(autoPause))
        verify(accountManager).register(any(), eq(lifecycleOwner), eq(autoPause))
    }

    @Test
    fun `GIVEN sync observer WHEN onStarted observed THEN sync status updated`() {
        syncObserver.onStarted()

        store.waitUntilIdle()
        assertEquals(SyncStatus.Started, store.state.status)
    }

    @Test
    fun `GIVEN sync observer WHEN onIdle observed THEN sync status updated`() {
        syncObserver.onIdle()

        store.waitUntilIdle()
        assertEquals(SyncStatus.Idle, store.state.status)
    }

    @Test
    fun `GIVEN sync observer WHEN onError observed THEN sync status updated`() {
        syncObserver.onError(Exception())

        store.waitUntilIdle()
        assertEquals(SyncStatus.Error, store.state.status)
    }

    @Test
    fun `GIVEN account observer WHEN onAuthenticated observed THEN device observer registered`() = runTest {
        val constellation = mock<DeviceConstellation>()
        val account = mock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(constellation)
        }

        accountObserver.onAuthenticated(account, mock<AuthType.Existing>())
        runCurrent()

        verify(constellation).registerDeviceObserver(constellationObserver, lifecycleOwner, autoPause)
    }

    @Test
    fun `GIVEN account observer WHEN onAuthenticated observed with profile THEN account and account state are updated`() = coroutineScope.runTest {
        val profile = generateProfile()
        val constellation = mock<DeviceConstellation>()
        val account = coMock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(constellation)
            whenever(getCurrentDeviceId()).thenReturn("id")
            whenever(getSessionToken()).thenReturn("token")
            whenever(getProfile()).thenReturn(profile)
        }

        assertEquals(AccountState.NotAuthenticated, store.state.accountState)

        accountObserver.onAuthenticated(account, mock<AuthType.Existing>())
        runCurrent()

        val expected = Account(
            profile.uid,
            profile.email,
            profile.avatar,
            profile.displayName,
            "id",
            "token",
        )
        store.waitUntilIdle()
        assertEquals(expected, store.state.account)
        assertEquals(AccountState.Authenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN account observer WHEN onAuthenticated observed without profile THEN account and account state are not updated`() = coroutineScope.runTest {
        val constellation = mock<DeviceConstellation>()
        val account = coMock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(constellation)
            whenever(getProfile()).thenReturn(null)
        }

        accountObserver.onAuthenticated(account, mock<AuthType.Existing>())
        runCurrent()

        store.waitUntilIdle()
        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN user is logged in WHEN onLoggedOut observed THEN sync status and account states are updated`() = coroutineScope.runTest {
        val account = coMock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(mock())
            whenever(getProfile()).thenReturn(null)
        }
        accountObserver.onAuthenticated(account, mock<AuthType.Existing>())
        runCurrent()

        accountObserver.onLoggedOut()
        runCurrent()

        store.waitUntilIdle()
        assertEquals(SyncStatus.LoggedOut, store.state.status)
        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN account observer WHEN onAuthenticationProblems observed THEN account state is updated`() = coroutineScope.runTest {
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)

        accountObserver.onAuthenticationProblems()
        runCurrent()

        store.waitUntilIdle()
        assertEquals(AccountState.AuthenticationProblem, store.state.accountState)
    }

    @Test
    fun `GIVEN account observer WHEN onFlowError observed THEN account state is updated`() = coroutineScope.runTest {
        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)

        accountObserver.onFlowError(mock<AuthFlowError>())
        runCurrent()

        store.waitUntilIdle()
        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN account observer WHEN onReady observed with profile THEN account states are updated`() = coroutineScope.runTest {
        val profile = generateProfile()
        val currentDeviceId = "id"
        val sessionToken = "token"
        val constellation = mock<DeviceConstellation>()
        val authenticatedAccount = coMock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(constellation)
            whenever(getCurrentDeviceId()).thenReturn(currentDeviceId)
            whenever(getSessionToken()).thenReturn(sessionToken)
            whenever(getProfile()).thenReturn(profile)
        }
        val account = Account(
            uid = profile.uid,
            email = profile.email,
            avatar = profile.avatar,
            displayName = profile.displayName,
            currentDeviceId = currentDeviceId,
            sessionToken = sessionToken,
        )

        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)

        `when`(authenticatedAccount.checkAuthorizationStatus(eq(SCOPE_PROFILE))).thenReturn(false)

        accountObserver.onReady(authenticatedAccount = authenticatedAccount)
        runCurrent()

        store.waitUntilIdle()
        assertEquals(account, store.state.account)
        assertEquals(AccountState.AuthenticationProblem, store.state.accountState)

        `when`(authenticatedAccount.checkAuthorizationStatus(eq(SCOPE_PROFILE))).thenReturn(true)

        accountObserver.onReady(authenticatedAccount = authenticatedAccount)
        runCurrent()

        store.waitUntilIdle()
        assertEquals(account, store.state.account)
        assertEquals(AccountState.Authenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN account observer WHEN onReady observed without profile THEN account states are not updated`() = coroutineScope.runTest {
        val constellation = mock<DeviceConstellation>()
        val account = coMock<OAuthAccount> {
            whenever(deviceConstellation()).thenReturn(constellation)
            whenever(getProfile()).thenReturn(null)
        }

        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)

        accountObserver.onReady(account)
        runCurrent()

        store.waitUntilIdle()
        assertNull(store.state.account)
        assertEquals(AccountState.NotAuthenticated, store.state.accountState)
    }

    @Test
    fun `GIVEN device observer WHEN onDevicesUpdate observed THEN constellation state updated`() {
        val constellation = mock<ConstellationState>()
        constellationObserver.onDevicesUpdate(constellation)

        store.waitUntilIdle()
        assertEquals(constellation, store.state.constellationState)
    }

    private fun generateProfile(
        uid: String = "uid",
        email: String = "email",
        avatar: Avatar = Avatar("url", true),
        displayName: String = "displayName",
    ) = Profile(uid, email, avatar, displayName)
}
