/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.`when`
import org.mockito.stubbing.OngoingStubbing

@ExperimentalCoroutinesApi // for runTestOnMain
class AutoPushObserverTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val manager: FxaAccountManager = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val pushFeature: AutoPushFeature = mock()

    @ExperimentalCoroutinesApi
    @Test
    fun `messages are forwarded to account manager`() = runTestOnMain {
        val observer = AutoPushObserver(manager, mock(), "test")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation).processRawEvent("foobar")
        Unit
    }

    @Test
    fun `account manager is not invoked if no account is available`() = runTestOnMain {
        val observer = AutoPushObserver(manager, mock(), "test")

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).setDevicePushSubscription(any())
        verify(constellation, never()).processRawEvent("foobar")
        Unit
    }

    @Test
    fun `messages are not forwarded to account manager if they are for a different scope`() = runTestOnMain {
        val observer = AutoPushObserver(manager, mock(), "fake")

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).processRawEvent(any())
        Unit
    }

    @ExperimentalCoroutinesApi
    @Test
    fun `subscription changes are forwarded to account manager`() = runTestOnMain {
        val observer = AutoPushObserver(manager, pushFeature, "test")

        whenSubscribe()

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)
        val state: ConstellationState = mock()
        val device: Device = mock()
        `when`(constellation.state()).thenReturn(state)
        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)

        observer.onSubscriptionChanged("test")

        verify(constellation).setDevicePushSubscription(any())
        Unit
    }

    @Test
    fun `do nothing if there is no account manager`() = runTestOnMain {
        val observer = AutoPushObserver(manager, pushFeature, "test")

        whenSubscribe()

        observer.onSubscriptionChanged("test")

        verify(constellation, never()).setDevicePushSubscription(any())
        Unit
    }

    @Test
    fun `subscription changes are not forwarded to account manager if they are for a different scope`() = runTestOnMain {
        val observer = AutoPushObserver(manager, mock(), "fake")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onSubscriptionChanged("test")

        verify(constellation, never()).setDevicePushSubscription(any())
        verifyNoInteractions(pushFeature)
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
                    appServerKey = null,
                ),
            )
        }
    }
}
