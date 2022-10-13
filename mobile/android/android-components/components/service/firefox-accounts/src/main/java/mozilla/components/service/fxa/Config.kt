/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider

typealias ServerConfig = mozilla.appservices.fxaclient.Config
typealias Server = mozilla.appservices.fxaclient.Config.Server

/**
 * @property periodMinutes How frequently periodic sync should happen.
 * @property initialDelayMinutes What should the initial delay for the periodic sync be.
 */
data class PeriodicSyncConfig(
    val periodMinutes: Int = 240,
    val initialDelayMinutes: Int = 5,
)

/**
 * Configuration for sync.
 *
 * @property supportedEngines A set of supported sync engines, exposed via [GlobalSyncableStoreProvider].
 * @property periodicSyncConfig Optional configuration for running sync periodically.
 * Periodic sync is disabled if this is `null`.
 */
data class SyncConfig(
    val supportedEngines: Set<SyncEngine>,
    val periodicSyncConfig: PeriodicSyncConfig?,
)

/**
 * Describes possible sync engines that device can support.
 *
 * @property nativeName Internally, Rust SyncManager represents engines as strings. Forward-compatibility
 * with new engines is one of the reasons for this. E.g. during any sync, an engine may appear that we
 * do not know about. At the public API level, we expose a concrete [SyncEngine] type to allow for more
 * robust integrations. We do not expose "unknown" engines via our public API, but do handle them
 * internally (by persisting their enabled/disabled status).
 *
 * [nativeName] must match engine strings defined in the sync15 crate, e.g. https://github.com/mozilla/application-services/blob/main/components/sync15/src/state.rs#L23-L38
 *
 * @property nativeName Name of the corresponding Sync1.5 collection.
*/
sealed class SyncEngine(val nativeName: String) {
    // NB: When adding new types, make sure to think through implications for the SyncManager.
    // See https://github.com/mozilla-mobile/android-components/issues/4557

    /**
     * A history engine.
     */
    object History : SyncEngine("history")

    /**
     * A bookmarks engine.
     */
    object Bookmarks : SyncEngine("bookmarks")

    /**
     * A 'logins/passwords' engine.
     */
    object Passwords : SyncEngine("passwords")

    /**
     * A remote tabs engine.
     */
    object Tabs : SyncEngine("tabs")

    /**
     * A credit cards engine.
     */
    object CreditCards : SyncEngine("creditcards")

    /**
     * An addresses engine.
     */
    object Addresses : SyncEngine("addresses")

    /**
     * An engine that's none of the above, described by [name].
     */
    data class Other(val name: String) : SyncEngine(name)

    /**
     * This engine is used internally, but hidden from the public API because we don't fully support
     * this data type right now.
     */
    internal object Forms : SyncEngine("forms")
}
