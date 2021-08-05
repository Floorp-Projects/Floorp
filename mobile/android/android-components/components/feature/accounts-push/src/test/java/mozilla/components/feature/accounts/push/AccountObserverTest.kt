/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushConfig
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.`when`
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.stubbing.OngoingStubbing
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AccountObserverTest {

    private val accountManager: FxaAccountManager = mock()
    private val pushFeature: AutoPushFeature = mock()
    private val pushScope: String = "testScope"
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val config: PushConfig = mock()
    private val crashReporter: CrashReporting = mock()

    @Before
    fun setup() {
        `when`(accountManager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)
        `when`(pushFeature.config).thenReturn(config)
    }

    @Test
    fun `register device observer for existing accounts`() {
        val lifecycle: Lifecycle = mock()
        val lifecycleOwner: LifecycleOwner = mock()
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            lifecycleOwner,
            false
        )
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(lifecycleOwner.lifecycle).thenReturn(lifecycle)

        observer.onAuthenticated(account, AuthType.Existing)

        verify(constellation).registerDeviceObserver(any(), eq(lifecycleOwner), anyBoolean())

        reset(constellation)

        observer.onAuthenticated(account, AuthType.Recovered)

        verify(constellation).registerDeviceObserver(any(), eq(lifecycleOwner), anyBoolean())
    }

    @Test
    fun `onLoggedOut removes cache`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        preference(testContext).edit()
            .putString(PREF_LAST_VERIFIED, "{\"timestamp\": 100, \"totalCount\": 0}")
            .putString(PREF_FXA_SCOPE, "12345")
            .apply()

        assertTrue(preference(testContext).contains(PREF_LAST_VERIFIED))

        observer.onLoggedOut()

        assertFalse(preference(testContext).contains(PREF_LAST_VERIFIED))
        assertTrue(preference(testContext).contains(PREF_FXA_SCOPE))
    }

    @Test
    fun `feature invoked on new account authenticated`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        observer.onAuthenticated(account, AuthType.Signin)

        verify(pushFeature).config
        verify(pushFeature).subscribe(eq(pushScope), nullable(), any(), any())
        verify(constellation).registerDeviceObserver(any(), any(), anyBoolean())

        verifyNoMoreInteractions(pushFeature)
    }

    @Test
    fun `feature and service are not invoked if not provided`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        observer.onAuthenticated(account, AuthType.Signup)
        observer.onLoggedOut()

        verify(pushFeature).subscribe(eq(pushScope), nullable(), any(), any())
        verify(constellation).registerDeviceObserver(any(), any(), anyBoolean())
    }

    @Test
    fun `feature does not subscribe if not a new account`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        observer.onAuthenticated(account, AuthType.Existing)

        verify(pushFeature).config

        verifyNoMoreInteractions(pushFeature)

        observer.onAuthenticated(account, AuthType.Recovered)

        verifyNoMoreInteractions(pushFeature)
    }

    @Test
    fun `notify account of new subscriptions`() = runBlocking {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        whenSubscribe()

        observer.onAuthenticated(account, AuthType.Signin)

        verify(constellation).setDevicePushSubscription(any())
        Unit
    }

    @Test
    fun `notify crash reporter on subscription error`() = runBlocking {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        whenSubscribeError()

        observer.onAuthenticated(account, AuthType.Signin)

        verify(crashReporter).recordCrashBreadcrumb(any())
        Unit
    }

    @Test
    fun `feature and service invoked on logout`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        observer.onLoggedOut()

        verify(pushFeature).unsubscribe(eq(pushScope), any(), any())
    }

    @Test
    fun `feature and service not invoked for any other callback`() {
        val observer = AccountObserver(
            testContext,
            pushFeature,
            pushScope,
            crashReporter,
            mock(),
            false
        )

        observer.onAuthenticationProblems()
        observer.onProfileUpdated(mock())

        verify(pushFeature).config
        verifyNoMoreInteractions(pushFeature)
    }

    @Suppress("UNCHECKED_CAST")
    private fun whenSubscribe(): OngoingStubbing<Unit>? {
        return `when`(pushFeature.subscribe(any(), nullable(), any(), any())).thenAnswer {

            // Invoke the `onSubscribe` lambda with a fake subscription.
            (it.arguments[3] as ((AutoPushSubscription) -> Unit)).invoke(
                AutoPushSubscription(
                    scope = "test",
                    endpoint = "https://foo",
                    publicKey = "p256dh",
                    authKey = "auth",
                    appServerKey = null
                )
            )
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun whenSubscribeError(): OngoingStubbing<Unit>? {
        return `when`(pushFeature.subscribe(any(), nullable(), any(), any())).thenAnswer {

            // Invoke the `onSubscribe` lambda with a fake subscription.
            (it.arguments[2] as ((Exception) -> Unit)).invoke(
                IllegalStateException("test")
            )
        }
    }
}
