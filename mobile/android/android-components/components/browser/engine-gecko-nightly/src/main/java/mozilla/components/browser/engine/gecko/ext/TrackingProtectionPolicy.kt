/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.ext

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.GeckoRuntimeSettings

/**
 * Converts a [TrackingProtectionPolicy] into a GeckoView setting that can be used with [GeckoRuntimeSettings.Builder].
 */
fun TrackingProtectionPolicy.toContentBlockingSetting(
    safeBrowsingPolicy: Array<EngineSession.SafeBrowsingPolicy> = arrayOf(EngineSession.SafeBrowsingPolicy.RECOMMENDED)
) = ContentBlocking.Settings.Builder().apply {
    enhancedTrackingProtectionLevel(getEtpLevel())
    antiTracking(getAntiTrackingPolicy())
    cookieBehavior(cookiePolicy.id)
    cookiePurging(cookiePurging)
    safeBrowsing(safeBrowsingPolicy.sumBy { it.id })
    strictSocialTrackingProtection(getStrictSocialTrackingProtection())
}.build()

/**
 * Returns whether [TrackingCategory.STRICT] is enabled in the [TrackingProtectionPolicy].
 */
internal fun TrackingProtectionPolicy.getStrictSocialTrackingProtection(): Boolean {
    return strictSocialTrackingProtection ?: trackingCategories.contains(TrackingCategory.STRICT)
}

/**
 * Returns the [TrackingProtectionPolicy] categories as an Enhanced Tracking Protection level for GeckoView.
 */
internal fun TrackingProtectionPolicy.getEtpLevel(): Int {
    return when {
        trackingCategories.contains(TrackingCategory.NONE) -> ContentBlocking.EtpLevel.NONE
        else -> ContentBlocking.EtpLevel.STRICT
    }
}

/**
 * Returns the [TrackingProtectionPolicy] as a tracking policy for GeckoView.
 */
internal fun TrackingProtectionPolicy.getAntiTrackingPolicy(): Int {
    /**
     * The [TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES] is an
     * artificial category, created with the sole purpose of going around this bug
     * https://bugzilla.mozilla.org/show_bug.cgi?id=1579264, for this reason we have to
     * remove its value from the valid anti tracking categories, when is present.
     */
    val total = trackingCategories.sumBy { it.id }
    return if (contains(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)) {
        total - TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id
    } else {
        total
    }
}
