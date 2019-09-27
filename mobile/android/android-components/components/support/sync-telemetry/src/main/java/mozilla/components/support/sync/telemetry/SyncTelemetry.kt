/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.sync.telemetry

import mozilla.appservices.sync15.FailureName
import mozilla.appservices.sync15.FailureReason
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.components.service.glean.private.LabeledMetricType
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.support.sync.telemetry.GleanMetrics.BookmarksSync
import mozilla.components.support.sync.telemetry.GleanMetrics.HistorySync
import mozilla.components.support.sync.telemetry.GleanMetrics.Pings

const val MAX_FAILURE_REASON_LENGTH = 100

/**
 * Contains functionality necessary to process instances of [SyncTelemetryPing].
 */
object SyncTelemetry {
    /**
     * Processes a history-related ping information from the [ping].
     * @return 'false' if global error was encountered, 'true' otherwise.
     */
    @Suppress("ComplexMethod", "NestedBlockDepth")
    fun processHistoryPing(ping: SyncTelemetryPing, sendPing: () -> Unit = { Pings.historySync.send() }): Boolean {
        ping.syncs.forEach eachSync@{ sync ->
            sync.failureReason?.let {
                recordFailureReason(it, HistorySync.failureReason)
                sendPing()
                return false
            }
            sync.engines.forEach eachEngine@{ engine ->
                if (engine.name != "history") {
                    return@eachEngine
                }
                HistorySync.apply {
                    val base = BaseGleanSyncPing.fromEngineInfo(ping.uid, engine)
                    uid.set(base.uid)
                    startedAt.set(base.startedAt)
                    finishedAt.set(base.finishedAt)
                    if (base.applied > 0) {
                        // Since all Sync ping counters have `lifetime: ping`, and
                        // we send the ping immediately after, we don't need to
                        // reset the counters before calling `add`.
                        incoming["applied"].add(base.applied)
                    }
                    if (base.failedToApply > 0) {
                        incoming["failed_to_apply"].add(base.failedToApply)
                    }
                    if (base.reconciled > 0) {
                        incoming["reconciled"].add(base.reconciled)
                    }
                    if (base.uploaded > 0) {
                        outgoing["uploaded"].add(base.uploaded)
                    }
                    if (base.failedToUpload > 0) {
                        outgoing["failed_to_upload"].add(base.failedToUpload)
                    }
                    if (base.outgoingBatches > 0) {
                        outgoingBatches.add(base.outgoingBatches)
                    }
                    base.failureReason?.let {
                        recordFailureReason(it, failureReason)
                    }
                }
                sendPing()
            }
        }
        return true
    }

    /**
     * Processes a bookmarks-related ping information from the [ping].
     * @return 'false' if global error was encountered, 'true' otherwise.
     */
    @Suppress("ComplexMethod", "NestedBlockDepth")
    fun processBookmarksPing(ping: SyncTelemetryPing, sendPing: () -> Unit = { Pings.bookmarksSync.send() }): Boolean {
        // This function is almost identical to `recordHistoryPing`, with additional
        // reporting for validation problems. Unfortunately, since the
        // `BookmarksSync` and `HistorySync` metrics are two separate objects, we
        // can't factor this out into a generic function.
        ping.syncs.forEach eachSync@{ sync ->
            sync.failureReason?.let {
                // If the entire sync fails, don't try to unpack the ping; just
                // report the error and bail.
                recordFailureReason(it, BookmarksSync.failureReason)
                sendPing()
                return false
            }
            sync.engines.forEach eachEngine@{ engine ->
                if (engine.name != "bookmarks") {
                    return@eachEngine
                }
                BookmarksSync.apply {
                    val base = BaseGleanSyncPing.fromEngineInfo(ping.uid, engine)
                    uid.set(base.uid)
                    startedAt.set(base.startedAt)
                    finishedAt.set(base.finishedAt)
                    if (base.applied > 0) {
                        incoming["applied"].add(base.applied)
                    }
                    if (base.failedToApply > 0) {
                        incoming["failed_to_apply"].add(base.failedToApply)
                    }
                    if (base.reconciled > 0) {
                        incoming["reconciled"].add(base.reconciled)
                    }
                    if (base.uploaded > 0) {
                        outgoing["uploaded"].add(base.uploaded)
                    }
                    if (base.failedToUpload > 0) {
                        outgoing["failed_to_upload"].add(base.failedToUpload)
                    }
                    if (base.outgoingBatches > 0) {
                        outgoingBatches.add(base.outgoingBatches)
                    }
                    base.failureReason?.let {
                        recordFailureReason(it, failureReason)
                    }
                    engine.validation?.let {
                        it.problems.forEach {
                            remoteTreeProblems[it.name].add(it.count)
                        }
                    }
                }
                sendPing()
            }
        }
        return true
    }

    private fun recordFailureReason(reason: FailureReason, failureReasonMetric: LabeledMetricType<StringMetricType>) {
        val metric = when (reason.name) {
            FailureName.Other, FailureName.Unknown -> failureReasonMetric["other"]
            FailureName.Unexpected, FailureName.Http -> failureReasonMetric["unexpected"]
            FailureName.Auth -> failureReasonMetric["auth"]
            FailureName.Shutdown -> return
        }
        val message = reason.message ?: "Unexpected error: ${reason.code}"
        metric.set(message.take(MAX_FAILURE_REASON_LENGTH))
    }
}
