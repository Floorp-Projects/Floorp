/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.AD
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.ANALYTICS
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.SOCIAL
import mozilla.components.concept.engine.content.blocking.Tracker
import org.mozilla.fenix.ext.getHostFromUrl
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT
import java.util.EnumMap

/**
 * Sorts [Tracker]s into different buckets and exposes them as a map.
 */
class TrackerBuckets {

    private var trackers = emptyList<Tracker>()
    var buckets = emptyMap<TrackingProtectionCategory, List<String>>()
        private set

    /**
     * If [newTrackers] has changed since the last call,
     * update [buckets] based on the new trackers list.
     */
    fun updateIfNeeded(newTrackers: List<Tracker>) {
        if (newTrackers != trackers) {
            trackers = newTrackers
            buckets = putTrackersInBuckets(newTrackers)
        }
    }

    /**
     * Returns true if there are no trackers.
     */
    fun isEmpty() = buckets.isEmpty()

    /**
     * Gets the tracker URLs for a given category.
     */
    operator fun get(key: TrackingProtectionCategory) = buckets[key].orEmpty()

    companion object {

        private fun putTrackersInBuckets(
            list: List<Tracker>
        ): Map<TrackingProtectionCategory, List<String>> {
            val map = EnumMap<TrackingProtectionCategory, List<String>>(TrackingProtectionCategory::class.java)
            for (item in list) {
                when {
                    CRYPTOMINING in item.trackingCategories -> {
                        map[CRYPTOMINERS] = map[CRYPTOMINERS].orEmpty() +
                                (item.url.getHostFromUrl() ?: item.url)
                    }
                    FINGERPRINTING in item.trackingCategories -> {
                        map[FINGERPRINTERS] = map[FINGERPRINTERS].orEmpty() +
                                (item.url.getHostFromUrl() ?: item.url)
                    }
                    SOCIAL in item.trackingCategories -> {
                        map[SOCIAL_MEDIA_TRACKERS] = map[SOCIAL_MEDIA_TRACKERS].orEmpty() +
                                (item.url.getHostFromUrl() ?: item.url)
                    }
                    AD in item.trackingCategories ||
                            SOCIAL in item.trackingCategories ||
                            ANALYTICS in item.trackingCategories -> {
                        map[TRACKING_CONTENT] = map[TRACKING_CONTENT].orEmpty() +
                                (item.url.getHostFromUrl() ?: item.url)
                    }
                }
            }
            return map
        }
    }
}
