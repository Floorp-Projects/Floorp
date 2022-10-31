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
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.sync.telemetry.GleanMetrics.BookmarksSync
import mozilla.components.support.sync.telemetry.GleanMetrics.FxaTab
import mozilla.components.support.sync.telemetry.GleanMetrics.HistorySync
import mozilla.components.support.sync.telemetry.GleanMetrics.LoginsSync
import mozilla.components.support.sync.telemetry.GleanMetrics.Pings
import mozilla.components.support.sync.telemetry.GleanMetrics.Sync
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.util.Date
import java.util.UUID

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
        val noGlobalError = SyncTelemetry.processHistoryPing(
            SyncTelemetryPing(
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
                                validation = null,
                            ),
                            EngineInfo(
                                name = "history",
                                at = now,
                                took = 5000,
                                incoming = IncomingInfo(
                                    applied = 5,
                                    failed = 4,
                                    newFailed = 3,
                                    reconciled = 2,
                                ),
                                outgoing = listOf(
                                    OutgoingInfo(
                                        sent = 10,
                                        failed = 5,
                                    ),
                                    OutgoingInfo(
                                        sent = 4,
                                        failed = 2,
                                    ),
                                ),
                                failureReason = null,
                                validation = null,
                            ),
                        ),
                        failureReason = null,
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
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    HistorySync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now, startedAt.testGetValue()!!.asSeconds())
                        assertEquals(now + 5, finishedAt.testGetValue()!!.asSeconds())
                        assertEquals(5, incoming["applied"].testGetValue())
                        assertEquals(7, incoming["failed_to_apply"].testGetValue())
                        assertEquals(2, incoming["reconciled"].testGetValue())
                        assertEquals(14, outgoing["uploaded"].testGetValue())
                        assertEquals(7, outgoing["failed_to_upload"].testGetValue())
                        assertEquals(2, outgoingBatches.testGetValue())
                        assertNull(Sync.syncUuid.testGetValue("history-sync"))
                    }
                }
                1 -> {
                    HistorySync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now + 10, startedAt.testGetValue()!!.asSeconds())
                        assertEquals(now + 15, finishedAt.testGetValue()!!.asSeconds())
                        assertTrue(
                            listOf(
                                incoming["applied"],
                                incoming["failed_to_apply"],
                                incoming["reconciled"],
                                outgoing["uploaded"],
                                outgoing["failed_to_upload"],
                                outgoingBatches,
                            ).none { it.testGetValue() != null },
                        )
                        assertNull(Sync.syncUuid.testGetValue("history-sync"))
                    }
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.submit()
            pingCount++
        }

        assertEquals(2, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends history telemetry pings on engine failure`() {
        val noGlobalError = SyncTelemetry.processHistoryPing(
            SyncTelemetryPing(
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
                                validation = null,
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
                                validation = null,
                            ),
                            EngineInfo(
                                name = "history",
                                at = now + 3,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "history",
                                at = now + 4,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Http, code = 418),
                                validation = null,
                            ),
                        ),
                        failureReason = null,
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
                                validation = null,
                            ),
                            EngineInfo(
                                name = "history",
                                at = now + 7,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "history",
                                at = now + 8,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    // Shutdown errors shouldn't be reported at all.
                    assertTrue(
                        listOf(
                            "other",
                            "unexpected",
                            "auth",
                        ).none { HistorySync.failureReason[it].testGetValue() != null },
                    )
                }
                1 -> HistorySync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                2 -> HistorySync.apply {
                    assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                3 -> HistorySync.apply {
                    assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                4 -> HistorySync.apply {
                    assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                5 -> HistorySync.apply {
                    assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.submit()
            pingCount++
        }

        assertEquals(6, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends history telemetry pings on sync failure`() {
        val noGlobalError = SyncTelemetry.processHistoryPing(
            SyncTelemetryPing(
                version = 1,
                uid = "abc123",
                syncs = listOf(
                    SyncInfo(
                        at = now,
                        took = 5000,
                        engines = emptyList(),
                        failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> HistorySync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("history-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.historySync.submit()
            pingCount++
        }

        assertEquals(1, pingCount)
        assertFalse(noGlobalError)
    }

    @Test
    fun `sends passwords telemetry pings on success`() {
        val noGlobalError = SyncTelemetry.processLoginsPing(
            SyncTelemetryPing(
                version = 1,
                uid = "abc123",
                syncs = listOf(
                    SyncInfo(
                        at = now,
                        took = 10000,
                        engines = listOf(
                            EngineInfo(
                                name = "history",
                                at = now + 5,
                                took = 5000,
                                incoming = IncomingInfo(
                                    applied = 10,
                                    failed = 2,
                                    newFailed = 3,
                                    reconciled = 2,
                                ),
                                outgoing = emptyList(),
                                failureReason = null,
                                validation = null,
                            ),
                            EngineInfo(
                                name = "passwords",
                                at = now,
                                took = 5000,
                                incoming = IncomingInfo(
                                    applied = 5,
                                    failed = 4,
                                    newFailed = 3,
                                    reconciled = 2,
                                ),
                                outgoing = listOf(
                                    OutgoingInfo(
                                        sent = 10,
                                        failed = 5,
                                    ),
                                    OutgoingInfo(
                                        sent = 4,
                                        failed = 2,
                                    ),
                                ),
                                failureReason = null,
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                    SyncInfo(
                        at = now + 10,
                        took = 5000,
                        engines = listOf(
                            EngineInfo(
                                name = "passwords",
                                at = now + 10,
                                took = 5000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = null,
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    LoginsSync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now, startedAt.testGetValue()!!.asSeconds())
                        assertEquals(now + 5, finishedAt.testGetValue()!!.asSeconds())
                        assertEquals(5, incoming["applied"].testGetValue())
                        assertEquals(7, incoming["failed_to_apply"].testGetValue())
                        assertEquals(2, incoming["reconciled"].testGetValue())
                        assertEquals(14, outgoing["uploaded"].testGetValue())
                        assertEquals(7, outgoing["failed_to_upload"].testGetValue())
                        assertEquals(2, outgoingBatches.testGetValue())
                        assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                    }
                }
                1 -> {
                    LoginsSync.apply {
                        assertEquals("abc123", uid.testGetValue())
                        assertEquals(now + 10, startedAt.testGetValue()!!.asSeconds())
                        assertEquals(now + 15, finishedAt.testGetValue()!!.asSeconds())
                        assertTrue(
                            listOf(
                                incoming["applied"],
                                incoming["failed_to_apply"],
                                incoming["reconciled"],
                                outgoing["uploaded"],
                                outgoing["failed_to_upload"],
                                outgoingBatches,
                            ).none { it.testGetValue() != null },
                        )
                        assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                    }
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendPasswordsPing`.
            Pings.loginsSync.submit()
            pingCount++
        }

        assertEquals(2, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends passwords telemetry pings on engine failure`() {
        val noGlobalError = SyncTelemetry.processLoginsPing(
            SyncTelemetryPing(
                version = 1,
                uid = "abc123",
                syncs = listOf(
                    SyncInfo(
                        at = now,
                        took = 5000,
                        engines = listOf(
                            // We should ignore any engines that aren't
                            // passwords.
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 1,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unknown, "Boxes not locked"),
                                validation = null,
                            ),
                            // Multiple passwords engine syncs per sync isn't
                            // expected, but it's easier to test the
                            // different failure types this way, instead of
                            // creating a top-level `SyncInfo` for each
                            // one.
                            EngineInfo(
                                name = "passwords",
                                at = now + 2,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Shutdown),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "passwords",
                                at = now + 3,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "passwords",
                                at = now + 4,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Http, code = 418),
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                    // ...But, just in case, we also test multiple top-level
                    // syncs.
                    SyncInfo(
                        at = now + 5,
                        took = 4000,
                        engines = listOf(
                            EngineInfo(
                                name = "passwords",
                                at = now + 6,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Auth, "Splines not reticulated", 999),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "passwords",
                                at = now + 7,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "passwords",
                                at = now + 8,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    // Shutdown errors shouldn't be reported at all.
                    assertTrue(
                        listOf(
                            "other",
                            "unexpected",
                            "auth",
                        ).none { LoginsSync.failureReason[it].testGetValue() != null },
                    )
                }
                1 -> LoginsSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                2 -> LoginsSync.apply {
                    assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                3 -> LoginsSync.apply {
                    assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                4 -> LoginsSync.apply {
                    assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                5 -> LoginsSync.apply {
                    assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendPasswordsPing`.
            Pings.loginsSync.submit()
            pingCount++
        }

        assertEquals(6, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends passwords telemetry pings on sync failure`() {
        val noGlobalError = SyncTelemetry.processLoginsPing(
            SyncTelemetryPing(
                version = 1,
                uid = "abc123",
                syncs = listOf(
                    SyncInfo(
                        at = now,
                        took = 5000,
                        engines = emptyList(),
                        failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> LoginsSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("logins-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.loginsSync.submit()
            pingCount++
        }

        assertEquals(1, pingCount)
        assertFalse(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on success`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(
            SyncTelemetryPing(
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
                                        failed = 5,
                                    ),
                                ),
                                failureReason = null,
                                validation = ValidationInfo(
                                    version = 2,
                                    problems = listOf(
                                        ProblemInfo(
                                            name = "missingParents",
                                            count = 5,
                                        ),
                                        ProblemInfo(
                                            name = "missingChildren",
                                            count = 7,
                                        ),
                                    ),
                                    failureReason = null,
                                ),
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    BookmarksSync.apply {
                        assertEquals("xyz789", uid.testGetValue())
                        assertEquals(now + 25, startedAt.testGetValue()!!.asSeconds())
                        assertEquals(now + 31, finishedAt.testGetValue()!!.asSeconds())
                        assertNull(incoming["applied"].testGetValue())
                        assertNull(incoming["failed_to_apply"].testGetValue())
                        assertNull(incoming["reconciled"].testGetValue())
                        assertEquals(10, outgoing["uploaded"].testGetValue())
                        assertEquals(5, outgoing["failed_to_upload"].testGetValue())
                        assertEquals(1, outgoingBatches.testGetValue())
                        assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                    }
                }
                else -> fail()
            }
            Pings.bookmarksSync.submit()
            pingCount++
        }

        assertEquals(pingCount, 1)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on engine failure`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(
            SyncTelemetryPing(
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
                                validation = null,
                            ),
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 2,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Shutdown),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 3,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 4,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Http, code = 418),
                                validation = null,
                            ),
                        ),
                        failureReason = null,
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
                                validation = null,
                            ),
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 7,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                                validation = null,
                            ),
                            EngineInfo(
                                name = "bookmarks",
                                at = now + 8,
                                took = 1000,
                                incoming = null,
                                outgoing = emptyList(),
                                failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                                validation = null,
                            ),
                        ),
                        failureReason = null,
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> {
                    // Shutdown errors shouldn't be reported.
                    assertTrue(
                        listOf(
                            "other",
                            "unexpected",
                            "auth",
                        ).none { BookmarksSync.failureReason[it].testGetValue() != null },
                    )
                }
                1 -> BookmarksSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                2 -> BookmarksSync.apply {
                    assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                3 -> BookmarksSync.apply {
                    assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                4 -> BookmarksSync.apply {
                    assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["other"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                5 -> BookmarksSync.apply {
                    assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendBookmarksPing`.
            Pings.bookmarksSync.submit()
            pingCount++
        }

        assertEquals(6, pingCount)
        assertTrue(noGlobalError)
    }

    @Test
    fun `sends bookmarks telemetry pings on sync failure`() {
        val noGlobalError = SyncTelemetry.processBookmarksPing(
            SyncTelemetryPing(
                version = 1,
                uid = "abc123",
                syncs = listOf(
                    SyncInfo(
                        at = now,
                        took = 5000,
                        engines = emptyList(),
                        failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                    ),
                ),
                events = emptyList(),
            ),
        ) {
            when (pingCount) {
                0 -> BookmarksSync.apply {
                    assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                    assertNull(failureReason["unexpected"].testGetValue())
                    assertNull(failureReason["auth"].testGetValue())
                    assertNull(Sync.syncUuid.testGetValue("bookmarks-sync"))
                }
                else -> fail()
            }
            // We still need to send the ping, so that the counters are
            // cleared out between calls to `sendHistoryPing`.
            Pings.bookmarksSync.submit()
            pingCount++
        }

        assertEquals(1, pingCount)
        assertFalse(noGlobalError)
    }

    @Test
    fun `sends a global sync ping alongside individual data type pings`() {
        val pings = mutableListOf<MutableMap<String, Int>>(HashMap())
        var globalPingCount = 0
        val globalSyncUuids = mutableListOf<UUID>()

        val syncTelemetry = SyncTelemetryPing(
            version = 1,
            uid = "abc123",
            syncs = listOf(
                SyncInfo(
                    at = now,
                    took = 10000,
                    engines = listOf(
                        EngineInfo(
                            name = "passwords",
                            at = now,
                            took = 5000,
                            incoming = IncomingInfo(
                                applied = 5,
                                failed = 4,
                                newFailed = 3,
                                reconciled = 2,
                            ),
                            outgoing = listOf(
                                OutgoingInfo(
                                    sent = 10,
                                    failed = 5,
                                ),
                                OutgoingInfo(
                                    sent = 4,
                                    failed = 2,
                                ),
                            ),
                            failureReason = null,
                            validation = null,
                        ),
                        EngineInfo(
                            name = "history",
                            at = now,
                            took = 5000,
                            incoming = IncomingInfo(
                                applied = 5,
                                failed = 4,
                                newFailed = 3,
                                reconciled = 2,
                            ),
                            outgoing = listOf(
                                OutgoingInfo(
                                    sent = 10,
                                    failed = 5,
                                ),
                                OutgoingInfo(
                                    sent = 4,
                                    failed = 2,
                                ),
                            ),
                            failureReason = null,
                            validation = null,
                        ),
                    ),
                    failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
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
                            validation = null,
                        ),
                    ),
                    failureReason = null,
                ),
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
                                    failed = 5,
                                ),
                            ),
                            failureReason = null,
                            validation = ValidationInfo(
                                version = 2,
                                problems = listOf(
                                    ProblemInfo(
                                        name = "missingParents",
                                        count = 5,
                                    ),
                                    ProblemInfo(
                                        name = "missingChildren",
                                        count = 7,
                                    ),
                                ),
                                failureReason = null,
                            ),
                        ),
                    ),
                    failureReason = null,
                ),
            ),
            events = emptyList(),
        )

        fun setOrAssertGlobalSyncUuid(currentPingIndex: Int, pingName: String) {
            if (globalSyncUuids.elementAtOrNull(currentPingIndex) == null) {
                globalSyncUuids.add(Sync.syncUuid.testGetValue(pingName)!!)
            } else {
                assertEquals(globalSyncUuids[currentPingIndex], Sync.syncUuid.testGetValue(pingName))
            }
        }

        fun setOrIncrementPingCount(currentPingIndex: Int, pingName: String) {
            if (pings.elementAtOrNull(currentPingIndex) == null) {
                pings.add(mutableMapOf(pingName to 1))
            } else {
                pings[currentPingIndex].incrementForKey(pingName)
            }
        }

        SyncTelemetry.processSyncTelemetry(
            syncTelemetry,
            submitGlobalPing = {
                assertNotNull(globalSyncUuids.elementAtOrNull(globalPingCount))
                assertEquals(globalSyncUuids[globalPingCount], Sync.syncUuid.testGetValue("sync"))

                // Assertions above already assert syncUuid; below, let's make sure that 'failureReason' is processed.
                when (globalPingCount) {
                    0 -> {
                        assertEquals("Synergies not aligned", Sync.failureReason["other"].testGetValue())
                    }
                    1 -> {
                        assertNull(Sync.failureReason["other"].testGetValue())
                    }
                    2 -> {
                        assertNull(Sync.failureReason["other"].testGetValue())
                    }
                    else -> fail()
                }

                Pings.sync.submit()
                globalPingCount++
            },
            submitHistoryPing = {
                when (val currentPingIndex = globalPingCount) {
                    0 -> {
                        setOrAssertGlobalSyncUuid(currentPingIndex, "history-sync")
                        setOrIncrementPingCount(currentPingIndex, "history")
                        HistorySync.apply {
                            assertEquals("abc123", uid.testGetValue())
                            assertEquals(now, startedAt.testGetValue()!!.asSeconds())
                            assertEquals(now + 5, finishedAt.testGetValue()!!.asSeconds())
                            assertEquals(5, incoming["applied"].testGetValue())
                            assertEquals(7, incoming["failed_to_apply"].testGetValue())
                            assertEquals(2, incoming["reconciled"].testGetValue())
                            assertEquals(14, outgoing["uploaded"].testGetValue())
                            assertEquals(7, outgoing["failed_to_upload"].testGetValue())
                            assertEquals(2, outgoingBatches.testGetValue())
                        }
                        Pings.historySync.submit()
                    }
                    1 -> {
                        setOrAssertGlobalSyncUuid(currentPingIndex, "history-sync")
                        setOrIncrementPingCount(currentPingIndex, "history")
                        HistorySync.apply {
                            assertEquals("abc123", uid.testGetValue())
                            assertEquals(now + 10, startedAt.testGetValue()!!.asSeconds())
                            assertEquals(now + 15, finishedAt.testGetValue()!!.asSeconds())
                            assertTrue(
                                listOf(
                                    incoming["applied"],
                                    incoming["failed_to_apply"],
                                    incoming["reconciled"],
                                    outgoing["uploaded"],
                                    outgoing["failed_to_upload"],
                                    outgoingBatches,
                                ).none { it.testGetValue() != null },
                            )
                        }
                        Pings.historySync.submit()
                    }
                    else -> fail()
                }
            },
            submitLoginsPing = {
                when (val currentPingIndex = globalPingCount) {
                    0 -> {
                        setOrAssertGlobalSyncUuid(currentPingIndex, "logins-sync")
                        setOrIncrementPingCount(currentPingIndex, "passwords")
                        LoginsSync.apply {
                            assertEquals("abc123", uid.testGetValue())
                            assertEquals(now, startedAt.testGetValue()!!.asSeconds())
                            assertEquals(now + 5, finishedAt.testGetValue()!!.asSeconds())
                            assertEquals(5, incoming["applied"].testGetValue())
                            assertEquals(7, incoming["failed_to_apply"].testGetValue())
                            assertEquals(2, incoming["reconciled"].testGetValue())
                            assertEquals(14, outgoing["uploaded"].testGetValue())
                            assertEquals(7, outgoing["failed_to_upload"].testGetValue())
                            assertEquals(2, outgoingBatches.testGetValue())
                        }
                        Pings.loginsSync.submit()
                    }
                    else -> fail()
                }
            },
            submitBookmarksPing = {
                when (val currentPingIndex = globalPingCount) {
                    2 -> {
                        setOrAssertGlobalSyncUuid(currentPingIndex, "bookmarks-sync")
                        setOrIncrementPingCount(currentPingIndex, "bookmarks")
                        BookmarksSync.apply {
                            assertEquals("abc123", uid.testGetValue())
                            assertEquals(now + 25, startedAt.testGetValue()!!.asSeconds())
                            assertEquals(now + 31, finishedAt.testGetValue()!!.asSeconds())
                            assertNull(incoming["applied"].testGetValue())
                            assertNull(incoming["failed_to_apply"].testGetValue())
                            assertNull(incoming["reconciled"].testGetValue())
                            assertEquals(10, outgoing["uploaded"].testGetValue())
                            assertEquals(5, outgoing["failed_to_upload"].testGetValue())
                            assertEquals(1, outgoingBatches.testGetValue())
                        }
                        Pings.bookmarksSync.submit()
                    }
                }
            },
        )

        assertEquals(
            listOf(
                mapOf("history" to 1, "passwords" to 1),
                mapOf("history" to 1),
                mapOf("bookmarks" to 1),
            ),
            pings,
        )
    }

    @Test
    fun `checks sent tab telemetry records what it should`() {
        val json = """
            {
                "commands_sent":[{
                    "flow_id":"test-flow-id",
                    "stream_id":"test-stream-id"
                }],
                "commands_received":[]
            }
        """
        SyncTelemetry.processFxaTelemetry(json)
        val events = FxaTab.sent.testGetValue()!!
        assertEquals(1, events.size)
        assertEquals("test-flow-id", events.elementAt(0).extra!!["flow_id"])
        assertEquals("test-stream-id", events.elementAt(0).extra!!["stream_id"])

        assertNull(FxaTab.received.testGetValue())
    }

    @Test
    fun `checks received tab telemetry records what it should`() {
        val json = """
            {
                "commands_received":[{
                    "flow_id":"test-flow-id",
                    "stream_id":"test-stream-id",
                    "reason":"test-reason"
                }]
            }
        """
        SyncTelemetry.processFxaTelemetry(json)
        val events = FxaTab.received.testGetValue()!!
        assertEquals(1, events.size)
        assertEquals("test-flow-id", events.elementAt(0).extra!!["flow_id"])
        assertEquals("test-stream-id", events.elementAt(0).extra!!["stream_id"])
        assertEquals("test-reason", events.elementAt(0).extra!!["reason"])

        assertNull(FxaTab.sent.testGetValue())
    }

    @Test
    fun `checks invalid tab telemetry doesn't record anything and doesn't crash`() {
        val crashReporter: CrashReporting = mock()
        // commands_sent is missing the stream_id, command_received is missing a reason
        val json = """
            {
                "commands_sent":[{
                    "flow_id":"test-flow-id"
                }],
                "commands_received":[{
                    "flow_id":"test-flow-id",
                    "stream_id":"test-stream-id"
                }]
            }
        """
        SyncTelemetry.processFxaTelemetry(json, crashReporter)
        // one exception for each of 'send' and 'received'
        verify(crashReporter, times(2)).submitCaughtException(any<JSONException>())
        // completely invalid json
        SyncTelemetry.processFxaTelemetry(""" foo bar """, crashReporter)
        assertNull(FxaTab.sent.testGetValue())
        assertNull(FxaTab.received.testGetValue())
        // One more exception, making it the 3rd time
        verify(crashReporter, times(3)).submitCaughtException(any<JSONException>())
    }

    private fun MutableMap<String, Int>.incrementForKey(key: String) {
        this[key] = 1 + this.getOrElse(key, { 0 })
    }
}
