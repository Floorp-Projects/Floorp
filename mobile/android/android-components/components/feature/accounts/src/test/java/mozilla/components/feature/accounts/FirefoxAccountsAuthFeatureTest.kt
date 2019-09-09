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
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.FxaAuthData
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

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

    @Test
    fun `auth interceptor`() {
        val manager = mock<FxaAccountManager>()
        val redirectUrl = "https://accounts.firefox.com/oauth/success/123"
        val feature = FirefoxAccountsAuthFeature(
            manager,
            redirectUrl,
            mock()
        ) { _, _ -> }

        // Non-final FxA url.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/not/the/right/url"))
        verify(manager, never()).finishAuthenticationAsync(any())

        // Non-FxA url.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://www.wikipedia.org"))
        verify(manager, never()).finishAuthenticationAsync(any())

        // Redirect url, without code/state.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/"))
        verify(manager, never()).finishAuthenticationAsync(any())

        // Redirect url, without code/state.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/test"))
        verify(manager, never()).finishAuthenticationAsync(any())

        // Code+state, no action.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode1&state=testState1")
        )
        verify(manager).finishAuthenticationAsync(
            FxaAuthData(authType = AuthType.OtherExternal(null), code = "testCode1", state = "testState1")
        )

        // Code+state, action=signin.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode2&state=testState2&action=signin")
        )
        verify(manager).finishAuthenticationAsync(
            FxaAuthData(authType = AuthType.Signin, code = "testCode2", state = "testState2")
        )

        // Code+state, action=signup.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode3&state=testState3&action=signup")
        )
        verify(manager).finishAuthenticationAsync(
            FxaAuthData(authType = AuthType.Signup, code = "testCode3", state = "testState3")
        )

        // Code+state, action=pairing.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode4&state=testState4&action=pairing")
        )
        verify(manager).finishAuthenticationAsync(
            FxaAuthData(authType = AuthType.Pairing, code = "testCode4", state = "testState4")
        )

        // Code+state, action is an unknown value.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode5&state=testState5&action=someNewActionType")
        )
        verify(manager).finishAuthenticationAsync(
            FxaAuthData(authType = AuthType.OtherExternal("someNewActionType"), code = "testCode5", state = "testState5")
        )
    }

    private fun prepareAccountManagerForSuccessfulAuthentication(): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfileAsync(anyBoolean())).thenReturn(CompletableDeferred(profile))
        `when`(mockAccount.beginOAuthFlowAsync(any())).thenReturn(CompletableDeferred(AuthFlowUrl("authState", "auth://url")))
        `when`(mockAccount.beginPairingFlowAsync(anyString(), any())).thenReturn(CompletableDeferred(AuthFlowUrl("authState", "auth://url")))
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