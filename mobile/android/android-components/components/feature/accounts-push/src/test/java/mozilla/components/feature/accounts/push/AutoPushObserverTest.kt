/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class AutoPushObserverTest {

    @Test
    fun `messages are forwarded to account manager`() {
        val manager: FxaAccountManager = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val observer = AutoPushObserver(manager, "test")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation).processRawEventAsync("foobar")
    }

    @Test
    fun `account manager is not invoked if no account is available`() {
        val manager: FxaAccountManager = mock()
        val constellation: DeviceConstellation = mock()
        val observer = AutoPushObserver(manager, "test")

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).setDevicePushSubscriptionAsync(any())
        verify(constellation, never()).processRawEventAsync("foobar")
    }

    @Test
    fun `messages are not forwarded to account manager if they are for a different scope`() {
        val manager: FxaAccountManager = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()
        val observer = AutoPushObserver(manager, "fake")

        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        observer.onMessageReceived("test", "foobar".toByteArray())

        verify(constellation, never()).processRawEventAsync(any())
    }
}
