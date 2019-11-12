/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
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
        buckets.updateIfNeeded(
            listOf(
                TrackerLog(
                    "http://facebook.com",
                    listOf(MOZILLA_SOCIAL)
                ),
                TrackerLog("https://google.com", listOf(), listOf(FINGERPRINTING)),
                TrackerLog("https://mozilla.com")
            )
        )

        assertEquals(listOf("google.com"), buckets.buckets.blockedBucketMap[FINGERPRINTERS])
        assertEquals(
            listOf("facebook.com"),
            buckets.buckets.loadedBucketMap[FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS]
        )
        assertTrue(buckets.buckets.blockedBucketMap[CRYPTOMINERS].isNullOrEmpty())
        assertTrue(buckets.buckets.loadedBucketMap[CRYPTOMINERS].isNullOrEmpty())
    }

    @Test
    fun `sorts trackers into bucket`() {
        val buckets = TrackerBuckets()
        buckets.updateIfNeeded(
            listOf(
                TrackerLog(
                    "http://facebook.com",
                    listOf(MOZILLA_SOCIAL)
                ),
                TrackerLog("https://google.com", listOf(), listOf(FINGERPRINTING)),
                TrackerLog("https://mozilla.com")
            )
        )

        assertEquals(
            mapOf(
                FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS to listOf("facebook.com")
            ), buckets.buckets.loadedBucketMap
        )

        assertEquals(
            mapOf(
                FINGERPRINTERS to listOf("google.com")
            ), buckets.buckets.blockedBucketMap
        )
    }

    @Test
    fun `trackers in the same site but with different categories`() {
        val buckets = TrackerBuckets()
        val acCategories = listOf(
            CRYPTOMINING,
            MOZILLA_SOCIAL,
            FINGERPRINTING,
            SCRIPTS_AND_SUB_RESOURCES
        )

        buckets.updateIfNeeded(
            listOf(
                TrackerLog(
                    url = "http://facebook.com",
                    cookiesHasBeenBlocked = true,
                    blockedCategories = acCategories,
                    loadedCategories = acCategories
                )
            )
        )

        val expectedBlockedMap =
            mapOf(
                FenixTrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS to listOf("facebook.com"),
                FenixTrackingProtectionCategory.TRACKING_CONTENT to listOf("facebook.com"),
                FenixTrackingProtectionCategory.FINGERPRINTERS to listOf("facebook.com"),
                FenixTrackingProtectionCategory.CRYPTOMINERS to listOf("facebook.com"),
                FenixTrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES to listOf("facebook.com")
            )
        val expectedLoadedMap =
            expectedBlockedMap - FenixTrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES

        assertEquals(expectedBlockedMap, buckets.buckets.blockedBucketMap)
        assertEquals(expectedLoadedMap, buckets.buckets.loadedBucketMap)
    }
}
