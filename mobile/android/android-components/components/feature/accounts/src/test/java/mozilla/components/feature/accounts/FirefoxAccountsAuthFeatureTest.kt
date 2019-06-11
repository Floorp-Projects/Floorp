/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.manager.DeviceTuple
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import androidx.test.ext.junit.runners.AndroidJUnit4

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: Config,
    scopes: Array<String>,
    val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, scopes, DeviceTuple("test", DeviceType.MOBILE, listOf()), null) {

    override fun createAccount(config: Config): OAuthAccount {
        return block()
    }
}

@RunWith(AndroidJUnit4::class)
class FirefoxAccountsAuthFeatureTest {

    @Test
    fun `begin authentication`() {
        val manager = prepareAccountManagerForSuccessfulAuthentication()
        val mockAddTab: TabsUseCases.AddNewTabUseCase = mock()
        val mockTabs: TabsUseCases = mock()
        `when`(mockTabs.addTab).thenReturn(mockAddTab)

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", this.coroutineContext)
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
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", this.coroutineContext)
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
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", this.coroutineContext)
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
            val feature = FirefoxAccountsAuthFeature(manager, mockTabs, "somePath", this.coroutineContext)
            feature.beginPairingAuthentication("auth://pair")
        }

        // Fallback url is invoked.
        verify(mockAddTab, times(1)).invoke("https://accounts.firefox.com/signin")
    }

    private fun prepareAccountManagerForSuccessfulAuthentication(): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))
        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))

        val manager = TestableFxaAccountManager(
            testContext,
            Config.release("dummyId", "bad://url"),
            arrayOf("profile", "test-scope")
        ) {
            mockAccount
        }

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }

    private fun prepareAccountManagerForFailedAuthentication(): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))

        `when`(mockAccount.beginOAuthFlowAsync(any(), anyBoolean())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))

        val manager = TestableFxaAccountManager(
            testContext,
            Config.release("dummyId", "bad://url"),
            arrayOf("profile", "test-scope")
        ) {
            mockAccount
        }

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }
}