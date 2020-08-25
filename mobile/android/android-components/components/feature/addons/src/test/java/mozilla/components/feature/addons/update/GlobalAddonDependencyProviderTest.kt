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
class GlobalAddonDependencyProviderTest {

    @Before
    fun before() {
        GlobalAddonDependencyProvider.updater = null
        GlobalAddonDependencyProvider.addonManager = null
    }

    @Test(expected = IllegalArgumentException::class)
    fun `requireAddonUpdater - without calling initialize`() {
        GlobalAddonDependencyProvider.requireAddonUpdater()
    }

    @Test(expected = IllegalArgumentException::class)
    fun `requireAddonManager - without calling initialize`() {
        GlobalAddonDependencyProvider.requireAddonManager()
    }

    @Test
    fun `requireAddonManager - after initialize`() {
        val manager = mock<AddonManager>()
        GlobalAddonDependencyProvider.initialize(manager, mock())
        assertEquals(manager, GlobalAddonDependencyProvider.requireAddonManager())
    }

    @Test
    fun `requireAddonUpdater - after initialize`() {
        val updater = mock<AddonUpdater>()
        GlobalAddonDependencyProvider.initialize(mock(), updater)
        assertEquals(updater, GlobalAddonDependencyProvider.requireAddonUpdater())
    }
}
