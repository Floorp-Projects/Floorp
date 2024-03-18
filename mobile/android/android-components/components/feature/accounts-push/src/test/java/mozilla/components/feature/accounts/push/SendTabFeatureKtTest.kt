/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi
class SendTabFeatureKtTest {
    @Test
    fun `feature register all observers`() = runTest {
        val accountManager: FxaAccountManager = mock()

        SendTabFeature(
            accountManager = accountManager,
            onTabsReceived = mock(),
        )

        verify(accountManager).registerForAccountEvents(any(), any(), anyBoolean())
    }

    @Test
    fun `feature registers only the device observer`() {
        val accountManager: FxaAccountManager = mock()

        SendTabFeature(
            accountManager = accountManager,
            onTabsReceived = mock(),
        )

        verify(accountManager).registerForAccountEvents(any(), any(), anyBoolean())

        verify(accountManager, never()).register(any(), any(), anyBoolean())
    }

    @Test
    fun `block is executed only account is available`() {
        val accountManager: FxaAccountManager = mock()
        val block: DeviceConstellation.() -> Unit = mock()
        val account: OAuthAccount = mock()
        val constellation: DeviceConstellation = mock()

        accountManager.withConstellation(block)

        verify(block, never()).invoke(constellation)

        `when`(accountManager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)

        accountManager.withConstellation(block)

        verify(block).invoke(constellation)
    }
}
