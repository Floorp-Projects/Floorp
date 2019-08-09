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
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.fxa.DeviceConfig
import org.junit.Assert.assertEquals

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: ServerConfig,
    scopes: Set<String>,
    val block: () -> OAuthAccount = { mock() }
) : FxaAccountManager(context, config, DeviceConfig("test", DeviceType.MOBILE, setOf()), null, scopes) {

    override fun createAccount(config: ServerConfig): OAuthAccount {
        return block()
    }
}

@RunWith(AndroidJUnit4::class)
class FirefoxAccountsAuthFeatureTest {
    class TestOnBeginAuthentication : (Context, String) -> Unit {
        var url: String? = null

        override fun invoke(context: Context, authUrl: String) {
            url = authUrl
        }
    }

    @Test
    fun `begin authentication`() {
        val manager = prepareAccountManagerForSuccessfulAuthentication()
        val authLabmda = TestOnBeginAuthentication()

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(
                manager,
                "somePath",
                this.coroutineContext,
                authLabmda
            )
            feature.beginAuthentication(testContext)
        }

        assertEquals("auth://url", authLabmda.url)
    }

    @Test
    fun `begin pairing authentication`() {
        val manager = prepareAccountManagerForSuccessfulAuthentication()
        val authLabmda = TestOnBeginAuthentication()

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(
                manager,
                "somePath",
                this.coroutineContext,
                authLabmda
            )
            feature.beginPairingAuthentication(testContext, "auth://pair")
        }

        assertEquals("auth://url", authLabmda.url)
    }

    @Test
    fun `begin authentication with errors`() {
        val manager = prepareAccountManagerForFailedAuthentication()
        val authLambda = TestOnBeginAuthentication()

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(
                manager,
                "somePath",
                this.coroutineContext,
                authLambda
            )
            feature.beginAuthentication(testContext)
        }

        // Fallback url is invoked.
        assertEquals("https://accounts.firefox.com/signin", authLambda.url)
    }

    @Test
    fun `begin pairing authentication with errors`() {
        val manager = prepareAccountManagerForFailedAuthentication()
        val authLambda = TestOnBeginAuthentication()

        runBlocking {
            val feature = FirefoxAccountsAuthFeature(
                manager,
                "somePath",
                this.coroutineContext,
                authLambda
            )
            feature.beginPairingAuthentication(testContext, "auth://pair")
        }

        // Fallback url is invoked.
        assertEquals("https://accounts.firefox.com/signin", authLambda.url)
    }

    private fun prepareAccountManagerForSuccessfulAuthentication(): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred("auth://url"))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig.release("dummyId", "bad://url"),
            setOf("test-scope")
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

        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred(value = null))
        `when`(mockAccount.completeOAuthFlowAsync(anyString(), anyString())).thenReturn(CompletableDeferred(true))

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig.release("dummyId", "bad://url"),
            setOf("test-scope")
        ) {
            mockAccount
        }

        runBlocking {
            manager.initAsync().await()
        }

        return manager
    }
}