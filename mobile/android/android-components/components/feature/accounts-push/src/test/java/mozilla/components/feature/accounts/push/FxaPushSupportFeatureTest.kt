/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FxaPushSupportFeatureTest {

    @Before
    fun setup() {
        preference(testContext).edit().remove(PREF_FXA_SCOPE).apply()
    }

    @Test
    fun `account observer registered`() {
        val accountManager: FxaAccountManager = mock()
        val pushFeature: AutoPushFeature = mock()

        FxaPushSupportFeature(testContext, accountManager, pushFeature)

        verify(accountManager).register(any())
        verify(pushFeature).register(any(), any(), anyBoolean())
    }

    @Test
    fun `feature generates and caches a scope`() {
        val accountManager: FxaAccountManager = mock()
        val pushFeature: AutoPushFeature = mock()

        FxaPushSupportFeature(testContext, accountManager, pushFeature)

        assertTrue(preference(testContext).contains(PREF_FXA_SCOPE))
    }

    @Test
    fun `feature does not generate a scope if one already exists`() {
        val accountManager: FxaAccountManager = mock()
        val pushFeature: AutoPushFeature = mock()

        preference(testContext).edit().putString(PREF_FXA_SCOPE, "testScope").apply()

        FxaPushSupportFeature(testContext, accountManager, pushFeature)

        val cachedScope = preference(testContext).getString(PREF_FXA_SCOPE, "")
        assertEquals("testScope", cachedScope!!)
    }
}