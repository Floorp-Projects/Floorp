/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicyForSessionTypes
import org.mozilla.fenix.Config
import org.mozilla.fenix.utils.Settings

/**
 * Handles the logic behind creating new [TrackingProtectionPolicy]s.
 */
class TrackingProtectionPolicyFactory(private val settings: Settings) {

    /**
     * Constructs a [TrackingProtectionPolicy] based on current preferences.
     *
     * @param normalMode whether or not tracking protection should be enabled
     * in normal browsing mode, defaults to the current preference value.
     * @param privateMode whether or not tracking protection should be enabled
     * in private browsing mode, default to the current preference value.
     * @return the constructed tracking protection policy based on preferences.
     */
    @Suppress("ComplexMethod")
    fun createTrackingProtectionPolicy(
        normalMode: Boolean = settings.shouldUseTrackingProtection,
        privateMode: Boolean = settings.shouldUseTrackingProtection
    ): TrackingProtectionPolicy {
        val trackingProtectionPolicy =
            when {
                settings.useStrictTrackingProtection -> TrackingProtectionPolicy.strict()
                settings.useCustomTrackingProtection -> return createCustomTrackingProtectionPolicy()
                else -> TrackingProtectionPolicy.recommended()
            }

        return when {
            normalMode && privateMode -> trackingProtectionPolicy.adaptPolicyToChannel()
            normalMode && !privateMode -> trackingProtectionPolicy.adaptPolicyToChannel().forRegularSessionsOnly()
            !normalMode && privateMode -> trackingProtectionPolicy.adaptPolicyToChannel().forPrivateSessionsOnly()
            else -> TrackingProtectionPolicy.none()
        }
    }

    private fun createCustomTrackingProtectionPolicy(): TrackingProtectionPolicy {
        return TrackingProtectionPolicy.select(
            cookiePolicy = getCustomCookiePolicy(),
            trackingCategories = getCustomTrackingCategories(),
            cookiePurging = Config.channel.isNightlyOrDebug
        ).let {
            if (settings.blockTrackingContentSelectionInCustomTrackingProtection == "private") {
                it.forPrivateSessionsOnly()
            } else {
                it
            }
        }
    }

    private fun getCustomCookiePolicy(): TrackingProtectionPolicy.CookiePolicy {
        return if (!settings.blockCookiesInCustomTrackingProtection) {
            TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL
        } else {
            when (settings.blockCookiesSelectionInCustomTrackingProtection) {
                "all" -> TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE
                "social" -> TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS
                "unvisited" -> TrackingProtectionPolicy.CookiePolicy.ACCEPT_VISITED
                "third-party" -> TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY
                else -> TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE
            }
        }
    }

    private fun getCustomTrackingCategories(): Array<TrackingProtectionPolicy.TrackingCategory> {
        val categories = arrayListOf(
            TrackingProtectionPolicy.TrackingCategory.AD,
            TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
            TrackingProtectionPolicy.TrackingCategory.SOCIAL,
            TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
        )

        if (settings.blockTrackingContentInCustomTrackingProtection) {
            categories.add(TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)
        }

        if (settings.blockFingerprintersInCustomTrackingProtection) {
            categories.add(TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING)
        }

        if (settings.blockCryptominersInCustomTrackingProtection) {
            categories.add(TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING)
        }

        return categories.toTypedArray()
    }
}

@VisibleForTesting
internal fun TrackingProtectionPolicyForSessionTypes.adaptPolicyToChannel(): TrackingProtectionPolicyForSessionTypes {
    return TrackingProtectionPolicy.select(
        trackingCategories = trackingCategories,
        cookiePolicy = cookiePolicy,
        strictSocialTrackingProtection = strictSocialTrackingProtection,
        cookiePurging = Config.channel.isNightlyOrDebug
    )
}
