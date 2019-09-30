/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider

typealias ServerConfig = mozilla.appservices.fxaclient.Config

/**
 * Configuration for the current device.
 *
 * @property name An initial name to use for the device record which will be created during authentication.
 * This can be changed later via [FxaDeviceConstellation].
 *
 * @property type Type of a device - mobile, desktop - used for displaying identifying icons on other devices.
 * This cannot be changed once device record is created.
 *
 * @property capabilities A set of device capabilities, such as SEND_TAB. This set can be expanded by
 * re-initializing [FxaAccountManager] with a new set (e.g. on app restart).
 * Shrinking a set of capabilities is currently not supported.
 */
data class DeviceConfig(
    val name: String,
    val type: DeviceType,
    val capabilities: Set<DeviceCapability>
)

/**
 * Configuration for sync.
 *
 * @property supportedEngines A set of supported sync engines, exposed via [GlobalSyncableStoreProvider].
 * @property syncPeriodInMinutes Optional, how frequently periodic sync should happen. If this is `null`,
 * periodic syncing will be disabled.
 */
data class SyncConfig(
    val supportedEngines: Set<SyncEngine>,
    val syncPeriodInMinutes: Long? = null
)

/**
 * Describes possible sync engines that device can support.
 *
 * @property nativeName Internally, Rust SyncManager represents engines as strings. Forward-compatibility
 * with new engines is one of the reasons for this. E.g. during any sync, an engine may appear that we
 * do not know about. At the public API level, we expose a concrete [SyncEngine] type to allow for more
 * robust integrations. We do not expose "unknown" engines via our public API, but do handle them
 * internally (by persisting their enabled/disabled status).
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
     * An engine that's none of the above, described by [name].
     */
    data class Other(val name: String) : SyncEngine(name)

    /**
     * This engine is used internally, but hidden from the public API because we don't fully support
     * this data type right now.
     */
    internal object Forms : SyncEngine("forms")
}
