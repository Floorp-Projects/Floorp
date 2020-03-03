/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.accounts.push.FxaPushSupportFeature.Companion.PUSH_SCOPE_PREFIX
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class OneTimeFxaPushResetTest {

    private val pushFeature: AutoPushFeature = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()

    @Before
    fun setup() {
        preference(testContext).edit().remove(PREF_FXA_SCOPE).apply()

        `when`(account.deviceConstellation()).thenReturn(constellation)
    }

    @Test
    fun `no changes are made if a scope does not exist`() {
        val reset = OneTimeFxaPushReset(testContext, pushFeature)

        reset.resetSubscriptionIfNeeded(account)

        assertNull(preference(testContext).getString(PREF_FXA_SCOPE, null))
        verifyZeroInteractions(account)
    }

    @Test
    fun `existing valid new scope format does nothing`() {
        val validPushScope = PUSH_SCOPE_PREFIX + "12345"
        preference(testContext).edit().putString(PREF_FXA_SCOPE, validPushScope).apply()

        val reset = OneTimeFxaPushReset(testContext, pushFeature)

        reset.resetSubscriptionIfNeeded(account)

        val testScope = preference(testContext).getString(PREF_FXA_SCOPE, null)
        assertNotNull(testScope)
        assertEquals(validPushScope, testScope)
        verifyZeroInteractions(account)
    }

    @Suppress("UNCHECKED_CAST")
    @Test
    fun `existing invalid scope format is updated`() {
        preference(testContext).edit().putString(PREF_FXA_SCOPE, "12345").apply()

        val validPushScope = PUSH_SCOPE_PREFIX + "12345"
        val reset = OneTimeFxaPushReset(testContext, pushFeature)

        `when`(pushFeature.subscribe(any(), nullable(), any(), any())).thenAnswer {

            // Invoke the `onSubscribe` lambda with a fake subscription.
            (it.arguments[3] as ((AutoPushSubscription) -> Unit)).invoke(
                AutoPushSubscription(
                    scope = validPushScope,
                    endpoint = "https://foo",
                    publicKey = "p256dh",
                    authKey = "auth",
                    appServerKey = null
                )
            )
        }

        reset.resetSubscriptionIfNeeded(account)

        verify(pushFeature).unsubscribe(eq("12345"), any(), any())
        verify(pushFeature).subscribe(eq(validPushScope), nullable(), any(), any())
        verify(constellation).setDevicePushSubscriptionAsync(any())
        assertEquals(validPushScope, preference(testContext).getString(PREF_FXA_SCOPE, null))
    }
}
