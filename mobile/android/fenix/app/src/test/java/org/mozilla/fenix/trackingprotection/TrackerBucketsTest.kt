/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.AD
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING
import mozilla.components.concept.engine.content.blocking.Tracker
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CRYPTOMINERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.FINGERPRINTERS
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.TRACKING_CONTENT

class TrackerBucketsTest {

    @Test
    fun `initializes with empty map`() {
        assertTrue(TrackerBuckets().isEmpty())
        assertTrue(TrackerBuckets().buckets.isEmpty())
    }

    @Test
    fun `getter accesses corresponding bucket`() {
        val buckets = TrackerBuckets()
        buckets.updateIfNeeded(listOf(
            Tracker("http://facebook.com", listOf(FINGERPRINTING, AD)),
            Tracker("https://google.com", listOf(AD)),
            Tracker("https://mozilla.com")
        ))

        assertEquals(listOf("google.com"), buckets[TRACKING_CONTENT])
        assertEquals(listOf("facebook.com"), buckets[FINGERPRINTERS])
        assertEquals(emptyList<String>(), buckets[CRYPTOMINERS])
    }

    @Test
    fun `sorts trackers into bucket`() {
        val buckets = TrackerBuckets()
        buckets.updateIfNeeded(listOf(
            Tracker("http://facebook.com", listOf(FINGERPRINTING, AD)),
            Tracker("https://google.com", listOf(AD)),
            Tracker("https://mozilla.com")
        ))

        assertEquals(mapOf(
            TRACKING_CONTENT to listOf("google.com"),
            FINGERPRINTERS to listOf("facebook.com")
        ), buckets.buckets)
    }
}
