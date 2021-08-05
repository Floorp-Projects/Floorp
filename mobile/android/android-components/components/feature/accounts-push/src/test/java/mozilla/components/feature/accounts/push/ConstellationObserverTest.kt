/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.push.exceptions.SubscriptionException
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
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class ConstellationObserverTest {

    private val push: AutoPushFeature = mock()
    private val verifier: VerificationDelegate = mock()
    private val state: ConstellationState = mock()
    private val device: Device = mock()
    private val context: Context = mock()
    private val account: OAuthAccount = mock()
    private val crashReporter: CrashReporting = mock()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `do nothing if subscription has not expired`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onDevicesUpdate(state)

        verifyNoInteractions(push)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(false)

        observer.onDevicesUpdate(state)

        verifyNoInteractions(push)
    }

    @Test
    fun `do nothing if verifier is false`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onDevicesUpdate(state)

        verifyNoInteractions(push)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(false)

        verifyNoInteractions(push)

        `when`(device.subscriptionExpired).thenReturn(true)

        observer.onDevicesUpdate(state)

        verifyNoInteractions(push)
    }

    @Test
    fun `invoke registration renewal`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(true)

        observer.onDevicesUpdate(state)

        verify(push).renewRegistration()
        verify(verifier).increment()
    }

    /**
     * Remove this test in the future. See [invoke registration renewal] test.
     */
    @Test
    @Ignore("If we don't fix #7143, we may need this.")
    fun `re-subscribe for push in onDevicesUpdate`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(true)

        observer.onDevicesUpdate(state)

        verify(push).unsubscribe(eq("testScope"), any(), any())
        verify(push).subscribe(eq("testScope"), any(), any(), any())
        verify(verifier).increment()
    }

    @Test
    fun `notify crash reporter if old and new subscription matches`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)
        val constellation: DeviceConstellation = mock()
        val state: ConstellationState = mock()
        val device: Device = mock()
        val subscription: DevicePushSubscription = mock()

        `when`(account.deviceConstellation()).thenReturn(constellation)
        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscription).thenReturn(subscription)
        `when`(subscription.endpoint).thenReturn("https://example.com/foobar")

        observer.onSubscribe(state, testSubscription())

        verify(crashReporter).submitCaughtException(any())
    }

    @Test
    fun `notify crash reporter if re-subscribe error occurs`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onSubscribeError(mock())

        verify(crashReporter).submitCaughtException(any<SubscriptionException>())
    }

    @Test
    fun `notify crash reporter if un-subscribe error occurs`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onUnsubscribeError(mock())

        verify(crashReporter).recordCrashBreadcrumb(any())
    }

    private fun testSubscription() = AutoPushSubscription(
        scope = "testScope",
        endpoint = "https://example.com/foobar",
        publicKey = "",
        authKey = "",
        appServerKey = null
    )
}
