/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.sync.telemetry

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.appservices.sync15.EngineInfo
import mozilla.appservices.sync15.FailureName
import mozilla.appservices.sync15.FailureReason
import mozilla.appservices.sync15.IncomingInfo
import mozilla.appservices.sync15.OutgoingInfo
import mozilla.appservices.sync15.ProblemInfo
import mozilla.appservices.sync15.SyncInfo
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.appservices.sync15.ValidationInfo
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.sync.telemetry.GleanMetrics.BookmarksSync
import mozilla.components.support.sync.telemetry.GleanMetrics.HistorySync
import mozilla.components.support.sync.telemetry.GleanMetrics.Pings
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Date

private fun Date.asSeconds() = time / BaseGleanSyncPing.MILLIS_PER_SEC

@RunWith(AndroidJUnit4::class)
class SyncTelemetryTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private var now: Long = 0
    private var pingCount = 0

    @Before
    fun setup() {
        now = Date().asSeconds()
        pingCount = 0
    }

    @Test
    fun `sends history telemetry pings on success`() {
        val noGlobalError = SyncTelemetry.processHistoryPing(SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 10000,
                    engines = listOf(
                        EngineInfo(
                            name = "logins",
                            at = now + 5,
                            took = 5000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = null,
                            validation = null
                        ),
                        EngineInfo(
                            name = "history",
                            at = now,
                            took = 5000,
                            incoming = IncomingInfo(
                                applied = 5,
                                failed = 4,
                                newFailed = 3,
                                reconciled = 2
                            ),
                            outgoing = listOf(
                                OutgoingInfo(
                                    sent = 10,
                                    failed = 5
                                ),
                                OutgoingInfo(
                                    sent = 4,
                                    failed = 2
                                )
                            ),
                            failureReason = null,
                            validation = null
                        )
                    ),
                    failureReason = null
                ),
                SyncInfo(
                    at = now + 10,
                    took = 5000,
                    engines = listOf(
                        EngineInfo(
                            name = "history",
                            at = now + 10,
                            took = 5000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = null,
                            validation = null
                        )
                    ),
                    failureReason = null
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> {
                    HistorySync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now, startedAt.testGetValue().asSeconds())
                        assertEquals(now + 5, finishedAt.testGetValue().asSeconds())
                        assertEquals(5, incoming["applied"].testGetValue())
                        assertEquals(7, incoming["failed_to_apply"].testGetValue())
                        assertEquals(2, incoming["reconciled"].testGetValue())
                        assertEquals(14, outgoing["uploaded"].testGetValue())
                        assertEquals(7, outgoing["failed_to_upload"].testGetValue())
                        assertEquals(2, outgoingBatches.testGetValue())
                    }
                }
                1 -> {
                    HistorySync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now + 10, startedAt.testGetValue().asSeconds())
                        assertEquals(now + 15, finishedAt.testGetValue().asSeconds())
                        assertTrue(listOf(
                            incoming["applied"],
                            incoming["failed_to_apply"],
                            incoming["reconciled"],
                            outgoing["uploaded"],
                            outgoing["failed_to_upload"],
                            outgoingBatches
                        ).none { it.testHasValue() })
                    }
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.send()
            pingCount++
        }

        assertEquals(2, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends history telemetry pings on engine failure`() {
        val noGlobalError = SyncTelemetry.processHistoryPing(SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 5000,
                    engines = listOf(
                        // We should ignore any engines that aren't
                        // history.
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 1,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unknown, "Boxes not locked"),
                            validation = null
                        ),
                        // Multiple history engine syncs per sync isn't
                        // expected, but it's easier to test the
                        // different failure types this way, instead of
                        // creating a top-level `SyncInfo` for each
                        // one.
                        EngineInfo(
                            name = "history",
                            at = now + 2,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Shutdown),
                            validation = null
                        ),
                        EngineInfo(
                            name = "history",
                            at = now + 3,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                            validation = null
                        ),
                        EngineInfo(
                            name = "history",
                            at = now + 4,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Http, code = 418),
                            validation = null
                        )
                    ),
                    failureReason = null
                ),
                // ...But, just in case, we also test multiple top-level
                // syncs.
                SyncInfo(
                    at = now + 5,
                    took = 4000,
                    engines = listOf(
                        EngineInfo(
                            name = "history",
                            at = now + 6,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Auth, "Splines not reticulated", 999),
                            validation = null
                        ),
                        EngineInfo(
                            name = "history",
                            at = now + 7,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                            validation = null
                        ),
                        EngineInfo(
                            name = "history",
                            at = now + 8,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                            validation = null
                        )
                    ),
                    failureReason = null
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> {
                    // Shutdown errors shouldn't be reported at all.
                    assertTrue(listOf(
                        "other",
                        "unexpected",
                        "auth"
                    ).none { HistorySync.failureReason[it].testHasValue() })
                }
                1 -> HistorySync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                2 -> HistorySync.apply {
                    assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                3 -> HistorySync.apply {
                    assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                }
                4 -> HistorySync.apply {
                    assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                5 -> HistorySync.apply {
                    assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.send()
            pingCount++
        }

        assertEquals(6, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends history telemetry pings on sync failure`() {
        val noGlobalError = SyncTelemetry.processHistoryPing(SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 5000,
                    engines = emptyList(),
                    failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned")
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> HistorySync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.send()
            pingCount++
        }

        assertEquals(1, pingCount)
        assertFalse(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on success`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(SyncTelemetryPing(
            version = 1,
            uid = "xyz789",
            syncs = listOf(
                SyncInfo(
                    at = now + 20,
                    took = 8000,
                    engines = listOf(
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 25,
                            took = 6000,
                            incoming = null,
                            outgoing = listOf(
                                OutgoingInfo(
                                    sent = 10,
                                    failed = 5
                                )
                            ),
                            failureReason = null,
                            validation = ValidationInfo(
                                version = 2,
                                problems = listOf(
                                    ProblemInfo(
                                        name = "missingParents",
                                        count = 5
                                    ),
                                    ProblemInfo(
                                        name = "missingChildren",
                                        count = 7
                                    )
                                ),
                                failureReason = null
                            )
                        )
                    ),
                    failureReason = null
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> {
                    BookmarksSync.apply {
                        assertEquals("xyz789", uid.testGetValue())
                        assertEquals(now + 25, startedAt.testGetValue().asSeconds())
                        assertEquals(now + 31, finishedAt.testGetValue().asSeconds())
                        assertFalse(incoming["applied"].testHasValue())
                        assertFalse(incoming["failed_to_apply"].testHasValue())
                        assertFalse(incoming["reconciled"].testHasValue())
                        assertEquals(10, outgoing["uploaded"].testGetValue())
                        assertEquals(5, outgoing["failed_to_upload"].testGetValue())
                        assertEquals(1, outgoingBatches.testGetValue())
                    }
                }
                else -> fail()
            }
            Pings.bookmarksSync.send()
            pingCount++
        }

        assertEquals(pingCount, 1)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on engine failure`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 5000,
                    engines = listOf(
                        EngineInfo(
                            name = "history",
                            at = now + 1,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unknown, "Boxes not locked"),
                            validation = null
                        ),
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 2,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Shutdown),
                            validation = null
                        ),
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 3,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                            validation = null
                        ),
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 4,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Http, code = 418),
                            validation = null
                        )
                    ),
                    failureReason = null
                ),
                SyncInfo(
                    at = now + 5,
                    took = 4000,
                    engines = listOf(
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 6,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Auth, "Splines not reticulated", 999),
                            validation = null
                        ),
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 7,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                            validation = null
                        ),
                        EngineInfo(
                            name = "bookmarks",
                            at = now + 8,
                            took = 1000,
                            incoming = null,
                            outgoing = emptyList(),
                            failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                            validation = null
                        )
                    ),
                    failureReason = null
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> {
                    // Shutdown errors shouldn't be reported.
                    assertTrue(listOf(
                        "other",
                        "unexpected",
                        "auth"
                    ).none { BookmarksSync.failureReason[it].testHasValue() })
                }
                1 -> BookmarksSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                2 -> BookmarksSync.apply {
                    assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                3 -> BookmarksSync.apply {
                    assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                }
                4 -> BookmarksSync.apply {
                    assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                    assertFalse(failureReason["other"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                5 -> BookmarksSync.apply {
                    assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendBookmarksPing`.
            Pings.bookmarksSync.send()
            pingCount++
        }

        assertEquals(6, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on sync failure`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 5000,
                    engines = emptyList(),
                    failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned")
                )
            ),
            events = emptyList()
        )) {
            when (pingCount) {
                0 -> BookmarksSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertFalse(failureReason["unexpected"].testHasValue())
                    assertFalse(failureReason["auth"].testHasValue())
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.bookmarksSync.send()
            pingCount++
        }

        assertEquals(1, pingCount)
        assertFalse(noGlobalError)
    }
}