/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * An [AbstractBinding] that invokes the [onSyncNow] callback when the [TabsTrayState.syncing] is
 * set.
 *
 * This binding is useful for connecting with [SyncedTabsView.Listener].
 */
@OptIn(ExperimentalCoroutinesApi::class)
class SyncButtonBinding(
    tabsTrayStore: TabsTrayStore,
    private val onSyncNow: () -> Unit
) : AbstractBinding<TabsTrayState>(tabsTrayStore) {
    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.syncing }
            .ifChanged()
            .collect { syncingNow ->
                if (syncingNow) {
                    onSyncNow()
                }
            }
    }
}
