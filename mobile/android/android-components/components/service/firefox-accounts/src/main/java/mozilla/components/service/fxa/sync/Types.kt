/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import mozilla.appservices.syncmanager.SyncAuthInfo
import mozilla.components.service.fxa.SyncEngine

/**
 * Converts from a list of raw strings describing engines to a set of [SyncEngine] objects.
 */
fun List<String>.toSyncEngines(): Set<SyncEngine> {
    return this.map { it.toSyncEngine() }.toSet()
}

/**
 * Conversion from our SyncAuthInfo into its "native" version used at the interface boundary.
 */
internal fun mozilla.components.concept.sync.SyncAuthInfo.toNative(): SyncAuthInfo {
    return SyncAuthInfo(
        kid = this.kid,
        fxaAccessToken = this.fxaAccessToken,
        syncKey = this.syncKey,
        tokenserverUrl = this.tokenServerUrl,
    )
}

internal fun String.toSyncEngine(): SyncEngine {
    return when (this) {
        "history" -> SyncEngine.History
        "bookmarks" -> SyncEngine.Bookmarks
        "passwords" -> SyncEngine.Passwords
        "tabs" -> SyncEngine.Tabs
        "creditcards" -> SyncEngine.CreditCards
        "addresses" -> SyncEngine.Addresses
        // This handles a case of engines that we don't yet model in SyncEngine.
        else -> SyncEngine.Other(this)
    }
}

internal fun SyncReason.toRustSyncReason(): mozilla.appservices.syncmanager.SyncReason {
    return when (this) {
        SyncReason.Startup -> mozilla.appservices.syncmanager.SyncReason.STARTUP
        SyncReason.User -> mozilla.appservices.syncmanager.SyncReason.USER
        SyncReason.Scheduled -> mozilla.appservices.syncmanager.SyncReason.SCHEDULED
        SyncReason.EngineChange -> mozilla.appservices.syncmanager.SyncReason.ENABLED_CHANGE
        SyncReason.FirstSync -> mozilla.appservices.syncmanager.SyncReason.ENABLED_CHANGE
    }
}

internal fun SyncReason.asString(): String {
    return when (this) {
        SyncReason.FirstSync -> "first_sync"
        SyncReason.Scheduled -> "scheduled"
        SyncReason.EngineChange -> "engine_change"
        SyncReason.User -> "user"
        SyncReason.Startup -> "startup"
    }
}

internal fun String.toSyncReason(): SyncReason {
    return when (this) {
        "startup" -> SyncReason.Startup
        "user" -> SyncReason.User
        "engine_change" -> SyncReason.EngineChange
        "scheduled" -> SyncReason.Scheduled
        "first_sync" -> SyncReason.FirstSync
        else -> throw IllegalStateException("Invalid SyncReason: $this")
    }
}
