/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager.ext

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class FxaAccountManagerKtTest {

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
