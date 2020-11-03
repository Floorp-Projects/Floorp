/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.setMain
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
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions

class ConstellationObserverTest {

    private val push: AutoPushFeature = mock()
    private val verifier: VerificationDelegate = mock()
    private val state: ConstellationState = mock()
    private val device: Device = mock()
    private val context: Context = mock()
    private val account: OAuthAccount = mock()
    private val crashReporter: CrashReporting = mock()
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
    }

    @Test
    fun `do nothing if subscription has not expired`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(false)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
    }

    @Test
    fun `do nothing if verifier is false`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(false)

        verifyZeroInteractions(push)

        `when`(device.subscriptionExpired).thenReturn(true)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
    }

    @Test
    @Ignore("Disabling the test until we revert the changes from #8846 and fix #7143")
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
    fun `re-subscribe for push in onDevicesUpdate`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(true)

        observer.onDevicesUpdate(state)

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
        `when`(subscription.endpoint).thenReturn("https://example.com")

        observer.onSubscribe(state, testSubscription())

        verify(crashReporter).submitCaughtException(any())
    }

    @Test
    fun `notify crash reporter if re-subscribe error occurs`() {
        val observer = ConstellationObserver(context, push, "testScope", account, verifier, crashReporter)

        observer.onSubscribeError(mock())

        verify(crashReporter).recordCrashBreadcrumb(any())
    }

    private fun testSubscription() = AutoPushSubscription(scope = "testScope", endpoint = "https://example.com", publicKey = "", authKey = "", appServerKey = null)
}
