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

                if (item.cookiesHasBeenBlocked) {
                    blockedMap[CROSS_SITE_TRACKING_COOKIES] =
                        blockedMap[CROSS_SITE_TRACKING_COOKIES].orEmpty() + item.url.tryGetHostFromUrl()
                }

                // Blocked categories
                bucketBlockedCategories(item, blockedMap)

                // Loaded categories
                bucketLoadedCategories(item, loadedMap)
            }
            return BucketedTrackerLog(blockedMap, loadedMap)
        }

        private fun bucketLoadedCategories(
            item: TrackerLog,
            loadedMap: EnumMap<TrackingProtectionCategory, List<String>>
        ) {
            item.loadedCategories.forEach { category ->
                if (CRYPTOMINING == category) {
                    loadedMap[CRYPTOMINERS] = loadedMap[CRYPTOMINERS].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
                if (FINGERPRINTING == category) {
                    loadedMap[FINGERPRINTERS] = loadedMap[FINGERPRINTERS].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
                if (MOZILLA_SOCIAL == category) {
                    loadedMap[SOCIAL_MEDIA_TRACKERS] =
                        loadedMap[SOCIAL_MEDIA_TRACKERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                }
                if (TrackingCategory.SCRIPTS_AND_SUB_RESOURCES == category) {
                    loadedMap[TRACKING_CONTENT] = loadedMap[TRACKING_CONTENT].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
            }
        }

        private fun bucketBlockedCategories(
            item: TrackerLog,
            blockedMap: EnumMap<TrackingProtectionCategory, List<String>>
        ) {
            item.blockedCategories.forEach { category ->
                if (CRYPTOMINING == category) {
                    blockedMap[CRYPTOMINERS] = blockedMap[CRYPTOMINERS].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
                if (FINGERPRINTING == category) {
                    blockedMap[FINGERPRINTERS] = blockedMap[FINGERPRINTERS].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
                if (MOZILLA_SOCIAL == category) {
                    blockedMap[SOCIAL_MEDIA_TRACKERS] =
                        blockedMap[SOCIAL_MEDIA_TRACKERS].orEmpty() +
                                item.url.tryGetHostFromUrl()
                }
                if (TrackingCategory.SCRIPTS_AND_SUB_RESOURCES == category) {
                    blockedMap[TRACKING_CONTENT] = blockedMap[TRACKING_CONTENT].orEmpty() +
                            item.url.tryGetHostFromUrl()
                }
            }
        }
    }
}
