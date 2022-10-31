/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.sync.telemetry

import mozilla.appservices.sync15.EngineInfo
import mozilla.appservices.sync15.FailureName
import mozilla.appservices.sync15.FailureReason
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.service.glean.private.LabeledMetricType
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.sync.telemetry.GleanMetrics.AddressesSync
import mozilla.components.support.sync.telemetry.GleanMetrics.BookmarksSync
import mozilla.components.support.sync.telemetry.GleanMetrics.CreditcardsSync
import mozilla.components.support.sync.telemetry.GleanMetrics.FxaTab
import mozilla.components.support.sync.telemetry.GleanMetrics.HistorySync
import mozilla.components.support.sync.telemetry.GleanMetrics.LoginsSync
import mozilla.components.support.sync.telemetry.GleanMetrics.Pings
import mozilla.components.support.sync.telemetry.GleanMetrics.Sync
import mozilla.components.support.sync.telemetry.GleanMetrics.TabsSync
import org.json.JSONException
import org.json.JSONObject

const val MAX_FAILURE_REASON_LENGTH = 100

// The exceptions we report to the crash reporter, but otherwise don't escape this module.
internal sealed class InvalidTelemetryException(cause: Exception) : Exception(cause) {
    // The top-level data passed in is invalid.
    class InvalidData(cause: JSONException) : InvalidTelemetryException(cause)

    // The sent or received tabs data is invalid.
    class InvalidEvents(cause: JSONException) : InvalidTelemetryException(cause)
}

/**
 * Contains functionality necessary to process instances of [SyncTelemetryPing].
 */
@Suppress("LargeClass")
object SyncTelemetry {
    private val logger = Logger("SyncTelemetry")

    /**
     * Process [SyncTelemetryPing] as returned from [mozilla.appservices.syncmanager.SyncManager].
     */
    @Suppress("LongParameterList")
    fun processSyncTelemetry(
        syncTelemetry: SyncTelemetryPing,

        // The following are present to make this function testable. In tests, we need to "intercept"
        // values set in singleton ping objects before they're reset by a 'submit' call.
        submitGlobalPing: () -> Unit = { Pings.sync.submit() },
        submitHistoryPing: () -> Unit = { Pings.historySync.submit() },
        submitBookmarksPing: () -> Unit = { Pings.bookmarksSync.submit() },
        submitLoginsPing: () -> Unit = { Pings.loginsSync.submit() },
        submitCreditCardsPing: () -> Unit = { Pings.creditcardsSync.submit() },
        submitAddressesPing: () -> Unit = { Pings.addressesSync.submit() },
        submitTabsPing: () -> Unit = { Pings.tabsSync.submit() },
    ) {
        syncTelemetry.syncs.forEach { syncInfo ->
            // Note that `syncUuid` is configured to be submitted in all of the sync pings (it's set
            // once, and will be attached by glean to history-sync, bookmarks-sync, and logins-sync pings).
            // However, this only happens if sync telemetry is being submitted via [processSyncTelemetry].
            // That is, if different data types were synchronized together, as happens when using a sync manager.
            // We can then use 'syncUuid' to associate together all of the individual syncs that happened together.
            // If a data type is synchronized individually via the legacy 'sync' API on specific storage layers,
            // then the corresponding ping will not have 'syncUuid' set.
            Sync.syncUuid.generateAndSet()

            // It's possible for us to sync some engines, and then get a hard error that fails the
            // entire sync. Examples of such errors are an HTTP server error, token authentication
            // error, or other kind of network error.
            // We can have some engines that succeed (and others that fail, with different reasons)
            // and still have a global failure_reason.
            syncInfo.failureReason?.let { failureReason ->
                recordFailureReason(failureReason, Sync.failureReason)
            }

            syncInfo.engines.forEach { engineInfo ->
                when (engineInfo.name) {
                    "bookmarks" -> {
                        individualBookmarksSync(syncTelemetry.uid, engineInfo)
                        submitBookmarksPing()
                    }
                    "history" -> {
                        individualHistorySync(syncTelemetry.uid, engineInfo)
                        submitHistoryPing()
                    }
                    "passwords" -> {
                        individualLoginsSync(syncTelemetry.uid, engineInfo)
                        submitLoginsPing()
                    }
                    "creditcards" -> {
                        individualCreditCardsSync(syncTelemetry.uid, engineInfo)
                        submitCreditCardsPing()
                    }
                    "addresses" -> {
                        individualAddressesSync(syncTelemetry.uid, engineInfo)
                        submitAddressesPing()
                    }
                    "tabs" -> {
                        individualTabsSync(syncTelemetry.uid, engineInfo)
                        submitTabsPing()
                    }
                    else -> logger.warn("Ignoring telemetry for engine ${engineInfo.name}")
                }
            }

            submitGlobalPing()
        }
    }

    /**
     * Processes a history-related ping information from the [ping].
     * @return 'false' if global error was encountered, 'true' otherwise.
     */
    @Suppress("ComplexMethod", "NestedBlockDepth")
    fun processHistoryPing(
        ping: SyncTelemetryPing,
        sendPing: () -> Unit = { Pings.historySync.submit() },
    ): Boolean {
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
                individualHistorySync(ping.uid, engine)
                sendPing()
            }
        }
        return true
    }

    /**
     * Processes a passwords-related ping information from the [ping].
     * @return 'false' if global error was encountered, 'true' otherwise.
     */
    @Suppress("ComplexMethod", "NestedBlockDepth")
    fun processLoginsPing(
        ping: SyncTelemetryPing,
        sendPing: () -> Unit = { Pings.loginsSync.submit() },
    ): Boolean {
        ping.syncs.forEach eachSync@{ sync ->
            sync.failureReason?.let {
                recordFailureReason(it, LoginsSync.failureReason)
                sendPing()
                return false
            }
            sync.engines.forEach eachEngine@{ engine ->
                if (engine.name != "passwords") {
                    return@eachEngine
                }
                individualLoginsSync(ping.uid, engine)
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
    fun processBookmarksPing(
        ping: SyncTelemetryPing,
        sendPing: () -> Unit = { Pings.bookmarksSync.submit() },
    ): Boolean {
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
                individualBookmarksSync(ping.uid, engine)
                sendPing()
            }
        }
        return true
    }

    @Suppress("ComplexMethod")
    private fun individualLoginsSync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "passwords") { "Expected 'passwords', got ${engineInfo.name}" }

        LoginsSync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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
    }

    @Suppress("ComplexMethod")
    private fun individualBookmarksSync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "bookmarks") { "Expected 'bookmarks', got ${engineInfo.name}" }

        BookmarksSync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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
            engineInfo.validation?.let {
                it.problems.forEach { problemInfo ->
                    remoteTreeProblems[problemInfo.name].add(problemInfo.count)
                }
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun individualHistorySync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "history") { "Expected 'history', got ${engineInfo.name}" }

        HistorySync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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
    }

    @Suppress("ComplexMethod")
    private fun individualCreditCardsSync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "creditcards") { "Expected 'creditcards', got ${engineInfo.name}" }

        CreditcardsSync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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
    }

    @Suppress("ComplexMethod")
    private fun individualAddressesSync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "addresses") { "Expected 'addresses', got ${engineInfo.name}" }

        AddressesSync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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
    }

    @Suppress("ComplexMethod")
    private fun individualTabsSync(hashedFxaUid: String, engineInfo: EngineInfo) {
        require(engineInfo.name == "tabs") { "Expected 'tabs', got ${engineInfo.name}" }

        TabsSync.apply {
            val base = BaseGleanSyncPing.fromEngineInfo(hashedFxaUid, engineInfo)
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

    fun processFxaTelemetry(jsonStr: String, crashReporter: CrashReporting? = null) {
        val json = try {
            JSONObject(jsonStr)
        } catch (e: JSONException) {
            crashReporter?.submitCaughtException(InvalidTelemetryException.InvalidData(e))
            logger.error("Invalid JSON in FxA telemetry", e)
            return
        }
        try {
            val sent = json.getJSONArray("commands_sent")
            for (i in 0..sent.length() - 1) {
                val one = sent.getJSONObject(i)
                val extras = FxaTab.SentExtra(
                    flowId = one.getString("flow_id"),
                    streamId = one.getString("stream_id"),
                )
                FxaTab.sent.record(extras)
            }
            logger.info("Reported telemetry for ${sent.length()} sent commands")
        } catch (e: JSONException) {
            crashReporter?.submitCaughtException(InvalidTelemetryException.InvalidEvents(e))
            logger.error("Failed to report sent commands", e)
        }
        try {
            val recd = json.getJSONArray("commands_received")
            for (i in 0..recd.length() - 1) {
                val one = recd.getJSONObject(i)
                val extras = FxaTab.ReceivedExtra(
                    flowId = one.getString("flow_id"),
                    streamId = one.getString("stream_id"),
                    reason = one.getString("reason"),
                )
                FxaTab.received.record(extras)
            }
            logger.info("Reported telemetry for ${recd.length()} received commands")
        } catch (e: JSONException) {
            crashReporter?.submitCaughtException(InvalidTelemetryException.InvalidEvents(e))
            logger.error("Failed to report received commands", e)
        }
    }
}
