/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import org.junit.Assert.assertEquals
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
        // TODO Add this when we have it from GV; https://bugzilla.mozilla.org/show_bug.cgi?id=1651454
        // assertEquals(policy.getStrictSocialTrackingProtection(), setting.strictSocialTrackingProtection)

        val policyWithSafeBrowsing = TrackingProtectionPolicy.recommended().toContentBlockingSetting(emptyArray())
        assertEquals(0, policyWithSafeBrowsing.safeBrowsingCategories)
    }

    @Test
    fun `getEtpLevel is always Strict unless None`() {
        assertEquals(EtpLevel.STRICT, TrackingProtectionPolicy.recommended().getEtpLevel())
        assertEquals(EtpLevel.STRICT, TrackingProtectionPolicy.strict().getEtpLevel())
        assertEquals(EtpLevel.NONE, TrackingProtectionPolicy.none().getEtpLevel())
    }
}
