/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.sync.telemetry

import mozilla.appservices.sync15.EngineInfo
import mozilla.appservices.sync15.FailureReason
import java.util.Date

/**
 * Holds fields common to all Glean sync engine pings.
 */
internal data class BaseGleanSyncPing(
    val uid: String,
    val startedAt: Date,
    val finishedAt: Date,
    val applied: Int,
    val failedToApply: Int,
    val reconciled: Int,
    val uploaded: Int,
    val failedToUpload: Int,
    val outgoingBatches: Int,
    val failureReason: FailureReason?,
) {
    companion object {
        const val MILLIS_PER_SEC = 1000L

        fun fromEngineInfo(uid: String, info: EngineInfo): BaseGleanSyncPing {
            val failedToApply = info.incoming?.let {
                it.failed + it.newFailed
            } ?: 0
            val (uploaded, failedToUpload) = info.outgoing.fold(Pair(0, 0)) { totals, batch ->
                val (uploaded, failedToUpload) = totals
                Pair(uploaded + batch.sent, failedToUpload + batch.failed)
            }
            return BaseGleanSyncPing(
                uid = uid,
                startedAt = Date(info.at * MILLIS_PER_SEC),
                // Glean intentionally doesn't support recording arbitrary
                // durations in timespans, and we can't use the timespan
                // measurement API because everything is measured in Rust
                // code. Instead, we record absolute start and end times.
                // The Sync ping records both `at` _and_ `took`, so this doesn't
                // leak additional info.
                finishedAt = Date(info.at * MILLIS_PER_SEC + info.took),
                applied = info.incoming?.applied ?: 0,
                failedToApply = failedToApply,
                reconciled = info.incoming?.reconciled ?: 0,
                uploaded = uploaded,
                failedToUpload = failedToUpload,
                outgoingBatches = info.outgoing.size,
                failureReason = info.failureReason,
            )
        }
    }
}
