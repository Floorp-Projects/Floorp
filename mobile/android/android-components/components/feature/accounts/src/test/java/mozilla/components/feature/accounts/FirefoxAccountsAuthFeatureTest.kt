/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceConfig
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.Server
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.StorageWrapper
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import kotlin.coroutines.CoroutineContext

internal class TestableStorageWrapper(
    manager: FxaAccountManager,
    accountEventObserverRegistry: ObserverRegistry<AccountEventsObserver>,
    serverConfig: ServerConfig,
    private val block: () -> OAuthAccount = {
        val account: OAuthAccount = mock()
        `when`(account.deviceConstellation()).thenReturn(mock())
        account
    },
) : StorageWrapper(manager, accountEventObserverRegistry, serverConfig) {
    override fun obtainAccount(): OAuthAccount = block()
}

// Same as the actual account manager, except we get to control how FirefoxAccountShaped instances
// are created. This is necessary because due to some build issues (native dependencies not available
// within the test environment) we can't use fxaclient supplied implementation of FirefoxAccountShaped.
// Instead, we express all of our account-related operations over an interface.
class TestableFxaAccountManager(
    context: Context,
    config: ServerConfig,
    scopes: Set<String>,
    coroutineContext: CoroutineContext,
    block: () -> OAuthAccount = { mock() },
) : FxaAccountManager(context, config, DeviceConfig("test", DeviceType.MOBILE, setOf()), null, scopes, null, coroutineContext) {
    private val testableStorageWrapper = TestableStorageWrapper(this, accountEventObserverRegistry, serverConfig, block)
    override fun getStorageWrapper(): StorageWrapper {
        return testableStorageWrapper
    }
}

@RunWith(AndroidJUnit4::class)
class FirefoxAccountsAuthFeatureTest {
    // Note that tests that involve secure storage specify API=21, because of issues testing secure storage on
    // 23+ API levels. See https://github.com/mozilla-mobile/android-components/issues/4956

    @Config(sdk = [22])
    @Test
    fun `begin authentication`() = runTest {
        val manager = prepareAccountManagerForSuccessfulAuthentication(
            this.coroutineContext,
        )
        val authUrl = CompletableDeferred<String>()
        val feature = FirefoxAccountsAuthFeature(
            manager,
            "somePath",
            this.coroutineContext,
        ) { _, url ->
            authUrl.complete(url)
        }
        feature.beginAuthentication(testContext)
        authUrl.await()
        assertEquals("auth://url", authUrl.getCompleted())
    }

    @Config(sdk = [22])
    @Test
    fun `begin pairing authentication`() = runTest {
        val manager = prepareAccountManagerForSuccessfulAuthentication(
            this.coroutineContext,
        )
        val authUrl = CompletableDeferred<String>()
        val feature = FirefoxAccountsAuthFeature(
            manager,
            "somePath",
            this.coroutineContext,
        ) { _, url ->
            authUrl.complete(url)
        }
        feature.beginPairingAuthentication(testContext, "auth://pair")
        authUrl.await()
        assertEquals("auth://url", authUrl.getCompleted())
    }

    @Config(sdk = [22])
    @Test
    fun `begin authentication with errors`() = runTest {
        val manager = prepareAccountManagerForFailedAuthentication(
            this.coroutineContext,
        )
        val authUrl = CompletableDeferred<String>()

        val feature = FirefoxAccountsAuthFeature(
            manager,
            "somePath",
            this.coroutineContext,
        ) { _, url ->
            authUrl.complete(url)
        }
        feature.beginAuthentication(testContext)
        authUrl.await()
        // Fallback url is invoked.
        assertEquals("https://accounts.firefox.com/signin", authUrl.getCompleted())
    }

    @Config(sdk = [22])
    @Test
    fun `begin pairing authentication with errors`() = runTest {
        val manager = prepareAccountManagerForFailedAuthentication(
            this.coroutineContext,
        )
        val authUrl = CompletableDeferred<String>()

        val feature = FirefoxAccountsAuthFeature(
            manager,
            "somePath",
            this.coroutineContext,
        ) { _, url ->
            authUrl.complete(url)
        }
        feature.beginPairingAuthentication(testContext, "auth://pair")
        authUrl.await()
        // Fallback url is invoked.
        assertEquals("https://accounts.firefox.com/signin", authUrl.getCompleted())
    }

    @Test
    fun `auth interceptor`() = runTest {
        val manager = mock<FxaAccountManager>()
        val redirectUrl = "https://accounts.firefox.com/oauth/success/123"
        val feature = FirefoxAccountsAuthFeature(
            manager,
            redirectUrl,
            mock(),
        ) { _, _ -> }

        // Non-final FxA url.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/not/the/right/url", null, false, false, false, false, false))
        verify(manager, never()).finishAuthentication(any())

        // Non-FxA url.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://www.wikipedia.org", null, false, false, false, false, false))
        verify(manager, never()).finishAuthentication(any())

        // Redirect url, without code/state.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/", null, false, false, false, false, false))
        verify(manager, never()).finishAuthentication(any())

        // Redirect url, without code/state.
        assertNull(feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/test", null, false, false, false, false, false))
        verify(manager, never()).finishAuthentication(any())

        // Code+state, no action.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode1&state=testState1", null, false, false, false, false, false),
        )

        shadowOf(getMainLooper()).idle()

        verify(manager).finishAuthentication(
            FxaAuthData(authType = AuthType.OtherExternal(null), code = "testCode1", state = "testState1"),
        )

        // Code+state, action=signin.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode2&state=testState2&action=signin", null, false, false, false, false, false),
        )

        shadowOf(getMainLooper()).idle()

        verify(manager).finishAuthentication(
            FxaAuthData(authType = AuthType.Signin, code = "testCode2", state = "testState2"),
        )

        // Code+state, action=signup.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode3&state=testState3&action=signup", null, false, false, false, false, false),
        )

        shadowOf(getMainLooper()).idle()

        verify(manager).finishAuthentication(
            FxaAuthData(authType = AuthType.Signup, code = "testCode3", state = "testState3"),
        )

        // Code+state, action=pairing.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode4&state=testState4&action=pairing", null, false, false, false, false, false),
        )

        shadowOf(getMainLooper()).idle()

        verify(manager).finishAuthentication(
            FxaAuthData(authType = AuthType.Pairing, code = "testCode4", state = "testState4"),
        )

        // Code+state, action is an unknown value.
        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(redirectUrl),
            feature.interceptor.onLoadRequest(mock(), "https://accounts.firefox.com/oauth/success/123/?code=testCode5&state=testState5&action=someNewActionType", null, false, false, false, false, false),
        )

        shadowOf(getMainLooper()).idle()

        verify(manager).finishAuthentication(
            FxaAuthData(authType = AuthType.OtherExternal("someNewActionType"), code = "testCode5", state = "testState5"),
        )
        Unit
    }

    @Config(sdk = [22])
    private suspend fun prepareAccountManagerForSuccessfulAuthentication(
        coroutineContext: CoroutineContext,
    ): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.deviceConstellation()).thenReturn(mock())
        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(profile)
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(AuthFlowUrl("authState", "auth://url"))
        `when`(mockAccount.beginPairingFlow(anyString(), any(), anyString())).thenReturn(AuthFlowUrl("authState", "auth://url"))
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            setOf("test-scope"),
            coroutineContext,
        ) {
            mockAccount
        }

        manager.start()

        return manager
    }

    @Config(sdk = [22])
    private suspend fun prepareAccountManagerForFailedAuthentication(
        coroutineContext: CoroutineContext,
    ): TestableFxaAccountManager {
        val mockAccount: OAuthAccount = mock()
        val profile = Profile(uid = "testUID", avatar = null, email = "test@example.com", displayName = "test profile")

        `when`(mockAccount.getProfile(anyBoolean())).thenReturn(profile)
        `when`(mockAccount.deviceConstellation()).thenReturn(mock())
        `when`(mockAccount.beginOAuthFlow(any(), anyString())).thenReturn(null)
        `when`(mockAccount.beginPairingFlow(anyString(), any(), anyString())).thenReturn(null)
        `when`(mockAccount.completeOAuthFlow(anyString(), anyString())).thenReturn(true)

        val manager = TestableFxaAccountManager(
            testContext,
            ServerConfig(Server.RELEASE, "dummyId", "bad://url"),
            setOf("test-scope"),
            coroutineContext,
        ) {
            mockAccount
        }

        manager.start()

        return manager
    }
}
