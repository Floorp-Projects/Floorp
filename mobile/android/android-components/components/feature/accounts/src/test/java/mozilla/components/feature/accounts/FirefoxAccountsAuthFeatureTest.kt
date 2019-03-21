/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.fxa.AccountStorage
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FxaAccountManager
import mozilla.components.service.fxa.FxaNetworkException
import mozilla.components.service.fxa.SharedPrefAccountStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test

import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: Config,
    scopes: Array<String>,
    accountStorage: AccountStorage = SharedPrefAccountStorage(context),
    val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, scopes, null) {
    override fun createAccount(config: Config): OAuthAccount {
        return block()
    }
}

@RunWith(RobolectricTestRunner::class)
class FirefoxAccountsAuthFeatureTest {

    @Test
    fun `begin authentication`() {
        val manager = prepareAccountManagerForSuccessfulAuthentication()
        val mockAddTab: TabsUseCases.AddNewTabUseCase = mock()
        val mockTabs: TabsUseCases = mock()
        `when`(mockTabs.addTab).thenReturn(mockAddTab)

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", "somePath", this.coroutineContext)
            feature.beginAuthentication()
        }

        verify(mockAddTab, times(1)).invoke("auth://url")
    }

    @Test
    fun `begin pairing authentication`() {
        val manager = prepareAccountManagerForSuccessfulAuthentication()
        val mockAddTab: TabsUseCases.AddNewTabUseCase = mock()
        val mockTabs: TabsUseCases = mock()
        `when`(mockTabs.addTab).thenReturn(mockAddTab)

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", "somePath", this.coroutineContext)
            feature.beginPairingAuthentication("auth://pair")
        }

        verify(mockAddTab, times(1)).invoke("auth://url")
    }

    @Test
    fun `begin authentication with errors`() {
        val manager = prepareAccountManagerForFailedAuthentication()
        val mockAddTab: TabsUseCases.AddNewTabUseCase = mock()
        val mockTabs: TabsUseCases = mock()
        `when`(mockTabs.addTab).thenReturn(mockAddTab)

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", "somePath", this.coroutineContext)
            feature.beginAuthentication()
        }

        // Fallback url is invoked.
        verify(mockAddTab, times(1)).invoke("https://accounts.firefox.com/signin")
    }

    @Test
    fun `begin pairing authentication with errors`() {
        val manager = prepareAccountManagerForFailedAuthentication()
        val mockAddTab: TabsUseCases.AddNewTabUseCase = mock()
        val mockTabs: TabsUseCases = mock()
        `when`(mockTabs.addTab).thenReturn(mockAddTab)

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", "somePath", this.coroutineContext)
            feature.beginPairingAuthentication("auth://pair")
        }

        // Fallback url is invoked.
        verify(mockAddTab, times(1)).invoke("https://accounts.firefox.com/signin")
    }

    private fun prepareAccountManagerForSuccessfulAuthentication(): TestableFxaAccountManager {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

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
            RuntimeEnvironment.application,
            Config.release("dummyId", "bad://url"),
            arrayOf("profile", "test-scope"),
            accountStorage
        ) {
            mockAccount
        }

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }

    private fun prepareAccountManagerForFailedAuthentication(): TestableFxaAccountManager {
        val accountStorage = mock<AccountStorage>()
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(CompletableDeferred(profile))

        val exceptionalDeferred = CompletableDeferred<String>()
        exceptionalDeferred.completeExceptionally(FxaNetworkException("oops"))
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
                RuntimeEnvironment.application,
                Config.release("dummyId", "bad://url"),
                arrayOf("profile", "test-scope"),
                accountStorage
        ) {
            mockAccount
        }

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }
}