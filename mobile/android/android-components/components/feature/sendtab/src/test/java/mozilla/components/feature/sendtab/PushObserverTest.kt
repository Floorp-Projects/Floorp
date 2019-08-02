/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.sendtab

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.feature.push.PushType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class PushObserverTest {

    @Test
    fun `subscription is forwarded to account manager`() {
        val manager: FxaAccountManager = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val observer = PushObserver(manager)
        val subscription = generateSubscription(PushType.Services)

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onSubscriptionAvailable(subscription)

        verify(constellation).setDevicePushSubscriptionAsync(any())
    }

    @Test
    fun `subscription is not forwarded if it's not for fxa`() {
        val manager: FxaAccountManager = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val observer = PushObserver(manager)
        val subscription = generateSubscription(PushType.WebPush)

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onSubscriptionAvailable(subscription)

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
    }

    @Test
    fun `messages are forwarded to account manager`() {
        val manager: FxaAccountManager = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val observer = PushObserver(manager)

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onEvent(PushType.Services, "foobar")

        verify(constellation).processRawEventAsync("foobar")
    }

    @Test
    fun `account manager is not invoked if no account is available`() {
        val manager: FxaAccountManager = mock()
        val constellation: DeviceConstellation = mock()
        val observer = PushObserver(manager)
        val subscription = generateSubscription(PushType.Services)

        observer.onSubscriptionAvailable(subscription)
        observer.onEvent(PushType.Services, "foobar")

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
        verify(constellation, never()).processRawEventAsync("foobar")
    }

    private fun generateSubscription(type: PushType) = AutoPushSubscription(
        type = type,
        endpoint = "https://endpoint.push.mozilla.com",
        publicKey = "1234",
        authKey = "myKey"
    )
}