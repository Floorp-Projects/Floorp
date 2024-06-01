/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

/**
 * A singleton that provides access to the synced tabs command queue.
 * This object lets the [SyncedTabsCommandsFlushWorker] access the queue.
 */
object GlobalSyncedTabsCommandsProvider {
    private var lazyCommands: Lazy<SyncedTabsCommands>? = null

    /**
     * Initializes this provider with a lazily-initialized
     * synced tabs command queue.
     *
     * @param value The lazily-initialized [SyncedTabsCommands] instance.
     */
    fun initialize(value: Lazy<SyncedTabsCommands>) {
        lazyCommands = value
    }

    internal fun requireCommands(): SyncedTabsCommands =
        requireNotNull(lazyCommands?.value) {
            "`GlobalSyncedTabsCommandsProvider.initialize` must be called before accessing `commands`"
        }
}
