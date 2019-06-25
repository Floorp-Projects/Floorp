/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")
package mozilla.components.browser.storage.sync

import mozilla.appservices.places.SyncAuthInfo
import java.util.Date
import mozilla.appservices.sync15.EngineInfo
import mozilla.appservices.sync15.FailureReason
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType

// We have type definitions at the concept level, and "external" types defined within Places.
// In practice these two types are largely the same, and this file is the conversion point.

/**
 * Conversion from our SyncAuthInfo into its "native" version used at the interface boundary.
 */
internal fun mozilla.components.concept.sync.SyncAuthInfo.into(): SyncAuthInfo {
    return SyncAuthInfo(
        kid = this.kid,
        fxaAccessToken = this.fxaAccessToken,
        syncKey = this.syncKey,
        tokenserverURL = this.tokenServerUrl
    )
}

/**
 * Conversion from a generic [VisitType] into its richer comrade within the 'places' lib.
 */
internal fun VisitType.into() = when (this) {
    VisitType.NOT_A_VISIT -> mozilla.appservices.places.VisitType.UPDATE_PLACE
    VisitType.LINK -> mozilla.appservices.places.VisitType.LINK
    VisitType.RELOAD -> mozilla.appservices.places.VisitType.RELOAD
    VisitType.TYPED -> mozilla.appservices.places.VisitType.TYPED
    VisitType.BOOKMARK -> mozilla.appservices.places.VisitType.BOOKMARK
    VisitType.EMBED -> mozilla.appservices.places.VisitType.EMBED
    VisitType.REDIRECT_PERMANENT -> mozilla.appservices.places.VisitType.REDIRECT_PERMANENT
    VisitType.REDIRECT_TEMPORARY -> mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY
    VisitType.DOWNLOAD -> mozilla.appservices.places.VisitType.DOWNLOAD
    VisitType.FRAMED_LINK -> mozilla.appservices.places.VisitType.FRAMED_LINK
}

internal fun mozilla.appservices.places.VisitType.into() = when (this) {
    mozilla.appservices.places.VisitType.UPDATE_PLACE -> VisitType.NOT_A_VISIT
    mozilla.appservices.places.VisitType.LINK -> VisitType.LINK
    mozilla.appservices.places.VisitType.RELOAD -> VisitType.RELOAD
    mozilla.appservices.places.VisitType.TYPED -> VisitType.TYPED
    mozilla.appservices.places.VisitType.BOOKMARK -> VisitType.BOOKMARK
    mozilla.appservices.places.VisitType.EMBED -> VisitType.EMBED
    mozilla.appservices.places.VisitType.REDIRECT_PERMANENT -> VisitType.REDIRECT_PERMANENT
    mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY -> VisitType.REDIRECT_TEMPORARY
    mozilla.appservices.places.VisitType.DOWNLOAD -> VisitType.DOWNLOAD
    mozilla.appservices.places.VisitType.FRAMED_LINK -> VisitType.FRAMED_LINK
}

internal fun mozilla.appservices.places.VisitInfo.into(): VisitInfo {
    return VisitInfo(
        url = this.url,
        title = this.title,
        visitTime = this.visitTime,
        visitType = this.visitType.into()
    )
}

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
    val failureReason: FailureReason?
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
                startedAt = Date(info.at.toLong() * MILLIS_PER_SEC),
                // Glean intentionally doesn't support recording arbitrary
                // durations in timespans, and we can't use the timespan
                // measurement API because everything is measured in Rust
                // code. Instead, we record absolute start and end times.
                // The Sync ping records both `at` _and_ `took`, so this doesn't
                // leak additional info.
                finishedAt = Date(info.at.toLong() * MILLIS_PER_SEC + info.took),
                applied = info.incoming?.applied ?: 0,
                failedToApply = failedToApply,
                reconciled = info.incoming?.reconciled ?: 0,
                uploaded = uploaded,
                failedToUpload = failedToUpload,
                outgoingBatches = info.outgoing.size,
                failureReason = info.failureReason
            )
        }
    }
}
