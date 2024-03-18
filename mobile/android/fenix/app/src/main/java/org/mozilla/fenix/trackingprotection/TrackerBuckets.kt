/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT
import java.util.EnumMap

typealias BucketMap = Map<TrackingProtectionCategory, List<TrackerLog>>

/**
 * Sorts [Tracker]s into different buckets and exposes them as a map.
 */
class TrackerBuckets {

    private var trackers = emptyList<TrackerLog>()

    data class BucketedTrackerLog(val blockedBucketMap: BucketMap, val loadedBucketMap: BucketMap)

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
    fun get(key: TrackingProtectionCategory, blocked: Boolean) =
        if (blocked) buckets.blockedBucketMap[key].orEmpty() else buckets.loadedBucketMap[key].orEmpty()

    companion object {

        private fun putTrackersInBuckets(
            list: List<TrackerLog>,
        ): BucketedTrackerLog {
            val blockedMap = createMap()
            val loadedMap = createMap()
            for (item in list) {
                if (item.cookiesHasBeenBlocked) {
                    blockedMap.addTrackerHost(CROSS_SITE_TRACKING_COOKIES, item)
                }

                // Blocked categories
                for (category in item.blockedCategories) {
                    blockedMap.addTrackerHost(category, item)
                }

                // Loaded categories
                for (category in item.loadedCategories) {
                    loadedMap.addTrackerHost(category, item)
                }
            }
            return BucketedTrackerLog(blockedMap, loadedMap)
        }

        /**
         * Create an empty mutable map of [TrackingProtectionCategory] to hostnames.
         */
        private fun createMap() =
            EnumMap<TrackingProtectionCategory, MutableList<TrackerLog>>(TrackingProtectionCategory::class.java)

        /**
         * Add the hostname of the [TrackerLog.url] into the map for the given category
         * from Android Components. The category is transformed into a corresponding Fenix bucket,
         * and the item is discarded if the category doesn't have a match.
         */
        private fun MutableMap<TrackingProtectionCategory, MutableList<TrackerLog>>.addTrackerHost(
            category: TrackingCategory,
            tracker: TrackerLog,
        ) {
            val key = when (category) {
                TrackingCategory.CRYPTOMINING -> CRYPTOMINERS
                TrackingCategory.FINGERPRINTING -> FINGERPRINTERS
                TrackingCategory.MOZILLA_SOCIAL -> SOCIAL_MEDIA_TRACKERS
                TrackingCategory.SCRIPTS_AND_SUB_RESOURCES -> TRACKING_CONTENT
                else -> return
            }
            addTrackerHost(key, tracker)
        }

        /**
         * Add the hostname of the [TrackerLog] into the map for the given [TrackingProtectionCategory].
         */
        private fun MutableMap<TrackingProtectionCategory, MutableList<TrackerLog>>.addTrackerHost(
            key: TrackingProtectionCategory,
            tracker: TrackerLog,
        ) {
            getOrPut(key) { mutableListOf() }.add(tracker)
        }
    }
}
