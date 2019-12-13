/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import mozilla.components.feature.addons.AddonManager
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class GlobalAddonManagerProviderTest {

    @Before
    fun before() {
        GlobalAddonManagerProvider.addonManager = null
    }

    @Test(expected = IllegalArgumentException::class)
    fun `requireAddonManager - without calling initialize`() {
        GlobalAddonManagerProvider.requireAddonManager()
    }

    @Test
    fun `requireAddonManager - after initialize`() {
        val manager = mock<AddonManager>()
        GlobalAddonManagerProvider.initialize(manager)
        assertEquals(manager, GlobalAddonManagerProvider.requireAddonManager())
    }
}
