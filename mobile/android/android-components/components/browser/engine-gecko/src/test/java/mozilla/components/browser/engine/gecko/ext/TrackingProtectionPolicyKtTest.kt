/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.geckoview.ContentBlocking.EtpLevel

class TrackingProtectionPolicyKtTest {

    private val defaultSafeBrowsing = arrayOf(EngineSession.SafeBrowsingPolicy.RECOMMENDED)

    @Test
    fun `transform the policy to a GeckoView ContentBlockingSetting`() {
        val policy = TrackingProtectionPolicy.recommended()
        val setting = policy.toContentBlockingSetting()

        assertEquals(policy.getEtpLevel(), setting.enhancedTrackingProtectionLevel)
        assertEquals(policy.getAntiTrackingPolicy(), setting.antiTrackingCategories)
        assertEquals(policy.cookiePolicy.id, setting.cookieBehavior)
        assertEquals(defaultSafeBrowsing.sumBy { it.id }, setting.safeBrowsingCategories)
        assertEquals(setting.strictSocialTrackingProtection, policy.strictSocialTrackingProtection)
        assertEquals(setting.cookiePurging, policy.cookiePurging)

        val policyWithSafeBrowsing = TrackingProtectionPolicy.recommended().toContentBlockingSetting(emptyArray())
        assertEquals(0, policyWithSafeBrowsing.safeBrowsingCategories)
    }

    @Test
    fun `getEtpLevel is always Strict unless None`() {
        assertEquals(EtpLevel.STRICT, TrackingProtectionPolicy.recommended().getEtpLevel())
        assertEquals(EtpLevel.STRICT, TrackingProtectionPolicy.strict().getEtpLevel())
        assertEquals(EtpLevel.NONE, TrackingProtectionPolicy.none().getEtpLevel())
    }

    @Test
    fun `getStrictSocialTrackingProtection is true if category is STRICT`() {
        val recommendedPolicy = TrackingProtectionPolicy.recommended()
        val strictPolicy = TrackingProtectionPolicy.strict()

        assertFalse(recommendedPolicy.toContentBlockingSetting().strictSocialTrackingProtection)
        assertTrue(strictPolicy.toContentBlockingSetting().strictSocialTrackingProtection)
    }
}
