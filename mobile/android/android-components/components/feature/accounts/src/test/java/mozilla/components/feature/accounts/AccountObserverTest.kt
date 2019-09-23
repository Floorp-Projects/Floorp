/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AccountObserverTest {

    private val context: Context = mock()
    private val accountManager: FxaAccountManager = mock()
    private val constellationObserver: DeviceConstellationObserver = mock()
    private val account: OAuthAccount = mock()

    @Test
    fun `register device observer for existing accounts`() {
        val lifecycle: Lifecycle = mock()
        val lifecycleOwner: LifecycleOwner = mock()
        val constellation: DeviceConstellation = mock()
        val observer = AccountObserver(testContext, accountManager, constellationObserver, lifecycleOwner, false)

        `when`(accountManager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(lifecycleOwner.lifecycle).thenReturn(lifecycle)

        observer.onAuthenticated(account, AuthType.Existing)

        verify(constellation).registerDeviceObserver(eq(constellationObserver), eq(lifecycleOwner), anyBoolean())
    }

    @Test
    fun `onLoggedOut removes cache`() {
        val observer = AccountObserver(testContext, accountManager, constellationObserver, mock(), false)

        preference(testContext).edit().putString(LAST_VERIFIED, "{\"timestamp\": 100, \"totalCount\": 0}").apply()

        assertTrue(preference(testContext).contains(LAST_VERIFIED))

        observer.onLoggedOut()

        assertFalse(preference(testContext).contains(LAST_VERIFIED))
    }
}