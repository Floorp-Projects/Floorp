/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS

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
            buckets.buckets.loadedBucketMap[TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS]
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
                TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS to listOf("facebook.com")
            ), buckets.buckets.loadedBucketMap
        )

        assertEquals(
            mapOf(
                FINGERPRINTERS to listOf("google.com")
            ), buckets.buckets.blockedBucketMap
        )
    }
}
