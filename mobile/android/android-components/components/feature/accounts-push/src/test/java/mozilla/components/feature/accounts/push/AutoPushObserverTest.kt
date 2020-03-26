/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.mockito.stubbing.OngoingStubbing

class AutoPushObserverTest {
    private val manager: FxaAccountManager = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val pushFeature: AutoPushFeature = mock()

    @Test
    fun `messages are forwarded to account manager`() {
        val observer = AutoPushObserver(manager, mock(), "test")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation).processRawEventAsync("foobar")
    }

    @Test
    fun `account manager is not invoked if no account is available`() {
        val observer = AutoPushObserver(manager, mock(), "test")

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
        verify(constellation, never()).processRawEventAsync("foobar")
    }

    @Test
    fun `messages are not forwarded to account manager if they are for a different scope`() {
        val observer = AutoPushObserver(manager, mock(), "fake")

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).processRawEventAsync(any())
    }

    @Test
    fun `subscription changes are forwarded to account manager`() {
        val observer = AutoPushObserver(manager, pushFeature, "test")

        whenSubscribe()

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onSubscriptionChanged("test")

        verify(constellation).setDevicePushSubscriptionAsync(any())
    }

    @Test
    fun `do nothing if there is no account manager`() {
        val observer = AutoPushObserver(manager, pushFeature, "test")

        whenSubscribe()

        observer.onSubscriptionChanged("test")

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
    }

    @Test
    fun `subscription changes are not forwarded to account manager if they are for a different scope`() {
        val observer = AutoPushObserver(manager, mock(), "fake")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onSubscriptionChanged("test")

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
        verifyZeroInteractions(pushFeature)
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
}
