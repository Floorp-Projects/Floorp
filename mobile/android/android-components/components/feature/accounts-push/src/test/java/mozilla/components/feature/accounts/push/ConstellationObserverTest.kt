/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DevicePushSubscription
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.`when`
import org.mockito.stubbing.OngoingStubbing

@ExperimentalCoroutinesApi // for runTestOnMain
@RunWith(AndroidJUnit4::class)
class ConstellationObserverTest {

    private val push: AutoPushFeature = mock()
    private val verifier: VerificationDelegate = mock()
    private val state: ConstellationState = mock()
    private val device: Device = mock()
    private val context: Context = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val crashReporter: CrashReporting = mock()

    @Before
    fun setup() {
        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(false)
        `when`(account.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.state()).thenReturn(state)
    }

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `first subscribe works`() = runTestOnMain {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        verifyNoInteractions(push)

        whenSubscribe()

        observer.onDevicesUpdate(state)

        verify(push).subscribe(eq("testScope"), any(), any(), any())
        verifyNoMoreInteractions(push)
        // We should have told the constellation of the new subscription.
        verify(constellation).setDevicePushSubscription(any())

        Unit
    }

    @Test
    fun `re-subscribe doesn't update constellation on same endpoint`() = runTestOnMain {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        verifyNoInteractions(push)

        whenAlreadySubscribed()
        whenSubscribe()

        observer.onDevicesUpdate(state)

        verify(push).subscribe(eq("testScope"), any(), any(), any())
        verifyNoMoreInteractions(push)
        // We should not have told the constellation of the subscription as it matches
        verify(constellation).state()
        verifyNoMoreInteractions(constellation)
        Unit
    }

    @Test
    fun `re-subscribe update constellations on same endpoint if expired`() = runTestOnMain {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        verifyNoInteractions(push)

        whenAlreadySubscribed(true)
        whenSubscribe()

        observer.onDevicesUpdate(state)

        verify(push).subscribe(eq("testScope"), any(), any(), any())
        verifyNoMoreInteractions(push)
        // We should have told the constellation of the same end-point subscription to clear the
        // expired flag on the server.
        verify(constellation).setDevicePushSubscription(any())
        Unit
    }

    // @Test
    fun `notify crash reporter if subscribe error occurs`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        whenSubscribeError()
        observer.onDevicesUpdate(state)

        verify(crashReporter).recordCrashBreadcrumb(any())
    }

    @Test
    fun `no FCM renewal if verifier is false`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        verifyNoInteractions(push)

        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(false)

        observer.onDevicesUpdate(state)

        // The verifier prevents us fetching a new FCM token but doesn't prevent
        // us calling .subscribe() on the push service.
        verify(push).subscribe(eq("testScope"), any(), any(), any())
        verifyNoMoreInteractions(push)
    }

    @Test
    fun `invoke registration renewal`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(true)

        observer.onDevicesUpdate(state)

        verify(push).renewRegistration()
        verify(verifier).increment()
    }

    private fun testSubscription() = AutoPushSubscription(
        scope = "testScope",
        endpoint = "https://example.com/foobar",
        publicKey = "",
        authKey = "",
        appServerKey = null,
    )

    @Suppress("UNCHECKED_CAST")
    private fun whenSubscribe(): OngoingStubbing<Unit>? {
        return `when`(push.subscribe(any(), nullable(), any(), any())).thenAnswer {
            // Invoke the `onSubscribe` lambda with a fake subscription.
            (it.arguments[3] as ((AutoPushSubscription) -> Unit)).invoke(
                testSubscription(),
            )
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun whenSubscribeError(): OngoingStubbing<Unit>? {
        return `when`(push.subscribe(any(), nullable(), any(), any())).thenAnswer {
            // Invoke the `onSubscribeError` lambda with a fake exception.
            (it.arguments[2] as ((Exception) -> Unit)).invoke(
                IllegalStateException("test"),
            )
        }
    }

    private fun whenAlreadySubscribed(expired: Boolean = false) {
        val subscription: DevicePushSubscription = mock()
        `when`(device.subscriptionExpired).thenReturn(expired)
        `when`(device.subscription).thenReturn(subscription)
        `when`(subscription.endpoint).thenReturn(testSubscription().endpoint)
    }
}
