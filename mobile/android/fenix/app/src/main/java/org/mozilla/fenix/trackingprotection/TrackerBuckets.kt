/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.mozilla.fenix.ext.tryGetHostFromUrl
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT
import java.util.EnumMap

typealias BucketMap = Map<TrackingProtectionCategory, List<String>>

/**
 * Sorts [Tracker]s into different buckets and exposes them as a map.
 */
class TrackerBuckets {

    private var trackers = emptyList<TrackerLog>()

    data class BucketedTrackerLog(var blockedBucketMap: BucketMap, var loadedBucketMap: BucketMap)

    var buckets: BucketedTrackerLog = BucketedTrackerLog(emptyMap(), emptyMap())
        private set

    /**
     * If [newTrackers] has changed since the last call,
     * update [buckets] based on the new tracker log list.
     */
    fun updateIfNeeded(newTrackers: List<TrackerLog>) {
        if (newTrackers != trackers) {
            trackers = newTrackers
            buckets = putTrackersInBuckets(newTrackers)
        }
    }

    /**
     * Returns true if there are no trackers being blocked.
     */
    fun blockedIsEmpty() = buckets.blockedBucketMap.isEmpty()

    /**
     * Returns true if there are no trackers loaded.
     */
    fun loadedIsEmpty() = buckets.loadedBucketMap.isEmpty()

    /**
     * Gets the tracker URLs for a given category.
     */
    operator fun get(key: TrackingProtectionCategory, blocked: Boolean) =
        if (blocked) buckets.blockedBucketMap[key].orEmpty() else buckets.loadedBucketMap[key].orEmpty()

    companion object {
        @Suppress("ComplexMethod")
        private fun putTrackersInBuckets(
            list: List<TrackerLog>
        ): BucketedTrackerLog {
            val blockedMap =
                EnumMap<TrackingProtectionCategory, List<String>>(TrackingProtectionCategory::class.java)
            val loadedMap =
                EnumMap<TrackingProtectionCategory, List<String>>(TrackingProtectionCategory::class.java)
            for (item in list) {
                when {
                    // Blocked categories
                    item.cookiesHasBeenBlocked -> {
                        blockedMap[CROSS_SITE_TRACKING_COOKIES] =
                            blockedMap[CROSS_SITE_TRACKING_COOKIES].orEmpty() + item.url.tryGetHostFromUrl()
                    }
                    CRYPTOMINING in item.blockedCategories -> {
                        blockedMap[CRYPTOMINERS] = blockedMap[CRYPTOMINERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                    FINGERPRINTING in item.blockedCategories -> {
                        blockedMap[FINGERPRINTERS] = blockedMap[FINGERPRINTERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                    MOZILLA_SOCIAL in item.blockedCategories -> {
                        blockedMap[SOCIAL_MEDIA_TRACKERS] =
                            blockedMap[SOCIAL_MEDIA_TRACKERS].orEmpty() +
                                    item.url.tryGetHostFromUrl()
                    }
                    TrackingCategory.SCRIPTS_AND_SUB_RESOURCES in item.blockedCategories -> {
                        blockedMap[TRACKING_CONTENT] = blockedMap[TRACKING_CONTENT].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                    // Loaded categories
                    CRYPTOMINING in item.loadedCategories -> {
                        loadedMap[CRYPTOMINERS] = loadedMap[CRYPTOMINERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                    FINGERPRINTING in item.loadedCategories -> {
                        loadedMap[FINGERPRINTERS] = loadedMap[FINGERPRINTERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                    MOZILLA_SOCIAL in item.loadedCategories -> {
                        loadedMap[SOCIAL_MEDIA_TRACKERS] =
                            loadedMap[SOCIAL_MEDIA_TRACKERS].orEmpty() +
                                    item.url.tryGetHostFromUrl()
                    }
                    TrackingCategory.SCRIPTS_AND_SUB_RESOURCES in item.loadedCategories -> {
                        loadedMap[TRACKING_CONTENT] = loadedMap[TRACKING_CONTENT].orEmpty() +
                                item.url.tryGetHostFromUrl()
                    }
                }
            }
            return BucketedTrackerLog(blockedMap, loadedMap)
        }
    }
}
