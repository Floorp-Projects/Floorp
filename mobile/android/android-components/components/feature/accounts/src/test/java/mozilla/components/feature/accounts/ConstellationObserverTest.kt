/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts

import android.content.Context
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions

class ConstellationObserverTest {

    private val push: PushProcessor = mock()
    private val verifier: VerificationDelegate = mock()
    private val state: ConstellationState = mock()
    private val device: Device = mock()
    private val context: Context = mock()

    @Test
    fun `do nothing if subscription has not expired`() {
        val observer = ConstellationObserver(context, push, verifier)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
        verifyZeroInteractions(verifier)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(false)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
    }

    @Test
    fun `do nothing if verifier is false`() {
        val observer = ConstellationObserver(context, push, verifier)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
        verifyZeroInteractions(verifier)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(false)

        verifyZeroInteractions(push)

        `when`(device.subscriptionExpired).thenReturn(true)

        observer.onDevicesUpdate(state)

        verifyZeroInteractions(push)
    }

    @Test
    fun `invoke registration renewal`() {
        val observer = ConstellationObserver(context, push, verifier)

        `when`(state.currentDevice).thenReturn(device)
        `when`(device.subscriptionExpired).thenReturn(true)
        `when`(verifier.allowedToRenew()).thenReturn(true)

        observer.onDevicesUpdate(state)

        verify(push).renewRegistration()
        verify(verifier).increment()
    }
}