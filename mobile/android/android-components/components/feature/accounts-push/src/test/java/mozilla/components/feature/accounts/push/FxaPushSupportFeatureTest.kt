/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import mozilla.components.feature.accounts.push.FxaPushSupportFeature.Companion.PUSH_SCOPE_PREFIX
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FxaPushSupportFeatureTest {

    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    private val pushFeature: AutoPushFeature = mock()
    private val accountManager: FxaAccountManager = mock()

    private lateinit var feature: FxaPushSupportFeature

    @Before
    fun setup() {
        preference(testContext).edit().remove(PREF_FXA_SCOPE).apply()
        `when`(pushFeature.config).thenReturn(mock())

        feature = FxaPushSupportFeature(
            context = testContext,
            accountManager = accountManager,
            pushFeature = pushFeature,
            crashReporter = null,
            coroutineScope = coroutineTestRule.scope,
        )
    }

    @Test
    fun `account observer registered`() {
        feature.initialize()

        verify(accountManager).register(any())
        verify(pushFeature).register(any(), any(), anyBoolean())
    }

    @Test
    fun `feature generates and caches a scope`() {
        feature.initialize()

        assertTrue(preference(testContext).contains(PREF_FXA_SCOPE))
    }

    @Test
    fun `feature does not generate a scope if one already exists`() {
        preference(testContext).edit().putString(PREF_FXA_SCOPE, "testScope").apply()

        feature.initialize()

        val cachedScope = preference(testContext).getString(PREF_FXA_SCOPE, "")
        assertEquals("testScope", cachedScope!!)
    }

    @Test
    fun `feature generates a partially predictable push scope`() {
        feature.initialize()

        val cachedScope = preference(testContext).getString(PREF_FXA_SCOPE, "")

        assertTrue(cachedScope!!.contains(PUSH_SCOPE_PREFIX))
    }
}
