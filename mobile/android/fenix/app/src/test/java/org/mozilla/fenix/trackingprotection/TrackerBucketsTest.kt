/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS

private typealias FenixTrackingProtectionCategory = TrackingProtectionCategory
class TrackerBucketsTest {

    @Test
    fun `initializes with empty map`() {
        assertTrue(TrackerBuckets().buckets.blockedBucketMap.isEmpty())
        assertTrue(TrackerBuckets().buckets.loadedBucketMap.isEmpty())
    }

    @Test
    fun `getter accesses corresponding bucket`() {
        val buckets = TrackerBuckets()
        val google = TrackerLog("https://google.com", listOf(), listOf(FINGERPRINTING))
        val facebook = TrackerLog("http://facebook.com", listOf(MOZILLA_SOCIAL))

        buckets.updateIfNeeded(
            listOf(
                google,
                facebook,
                TrackerLog("https://mozilla.com"),
            ),
        )

        assertEquals(google, buckets.buckets.blockedBucketMap[FINGERPRINTERS]!!.first())
        assertEquals(
            facebook,
            buckets.buckets.loadedBucketMap[FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS]!!.first(),
        )
        assertTrue(buckets.buckets.blockedBucketMap[CRYPTOMINERS].isNullOrEmpty())
        assertTrue(buckets.buckets.loadedBucketMap[CRYPTOMINERS].isNullOrEmpty())
    }

    @Test
    fun `sorts trackers into bucket`() {
        val buckets = TrackerBuckets()
        val google = TrackerLog("https://google.com", listOf(), listOf(FINGERPRINTING))
        val facebook = TrackerLog("http://facebook.com", listOf(MOZILLA_SOCIAL))
        val mozilla = TrackerLog("https://mozilla.com")
        buckets.updateIfNeeded(
            listOf(
                facebook,
                google,
                mozilla,
            ),
        )

        assertEquals(
            mapOf(
                FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS to listOf(facebook),
            ),
            buckets.buckets.loadedBucketMap,
        )

        assertEquals(
            mapOf(
                FINGERPRINTERS to listOf(google),
            ),
            buckets.buckets.blockedBucketMap,
        )
    }

    @Test
    fun `trackers in the same site but with different categories`() {
        val buckets = TrackerBuckets()
        val acCategories = listOf(
            CRYPTOMINING,
            MOZILLA_SOCIAL,
            FINGERPRINTING,
            SCRIPTS_AND_SUB_RESOURCES,
        )

        val trackerLog = TrackerLog(
            url = "http://facebook.com",
            cookiesHasBeenBlocked = true,
            blockedCategories = acCategories,
            loadedCategories = acCategories,
        )
        buckets.updateIfNeeded(listOf(trackerLog))

        val expectedBlockedMap =
            mapOf(
                FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS to listOf(trackerLog),
                FenixTrackingProtectionCategory.TRACKING_CONTENT to listOf(trackerLog),
                FenixTrackingProtectionCategory.FINGERPRINTERS to listOf(trackerLog),
                FenixTrackingProtectionCategory.CRYPTOMINERS to listOf(trackerLog),
                FenixTrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES to listOf(trackerLog),
            )
        val expectedLoadedMap =
            expectedBlockedMap - FenixTrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES

        assertEquals(expectedBlockedMap, buckets.buckets.blockedBucketMap)
        assertEquals(expectedLoadedMap, buckets.buckets.loadedBucketMap)
    }
}
